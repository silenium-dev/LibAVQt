// Copyright (c) 2022.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//
// Created by silas on 23.01.22.
//

#include "common/FBOPool.hpp"
#include "private/FBOPool_p.hpp"

#include <QtDebug>
#include <utility>

namespace AVQt::common {
    FBOPool::FBOPool(QSize fboSize, bool dynamic, size_t size, size_t maxSize)
        : m_pool(std::make_shared<FBOPoolPrivate>(this)) {
        Q_ASSERT(m_pool->maxSize >= m_pool->initialSize);

        m_pool->fboSize = fboSize;
        m_pool->dynamic = dynamic;
        m_pool->initialSize = size;
        m_pool->maxSize = dynamic ? maxSize : size;

        allocateNewFBOs(m_pool->initialSize);
    }

    void FBOPool::allocateNewFBOs(size_t count) {
        for (int i = 0; i < count; ++i) {
            auto fbo = std::make_unique<QOpenGLFramebufferObject>(m_pool->fboSize, QOpenGLFramebufferObject::CombinedDepthStencil);
            auto id = m_pool->fboPool_idCounter++;
            m_pool->fboPool_free.enqueue(id);
            m_pool->fboPool_all.emplace(id, std::move(fbo));
        }
    }

    std::shared_ptr<QOpenGLFramebufferObject> FBOPool::getFBO(QDeadlineTimer deadline) {
        QMutexLocker locker(&m_pool->poolMutex);
        if (m_pool->fboPool_free.isEmpty()) {
            auto toAllocate = qMin(m_pool->initialSize, m_pool->maxSize - m_pool->fboPool_all.size());
            if (m_pool->dynamic && toAllocate > 0) {
                allocateNewFBOs(toAllocate);
            } else {
                qDebug() << "No free FBOs available";
                m_pool->fboPool_available.wait(&m_pool->poolMutex, deadline);
                if (m_pool->fboPool_free.isEmpty()) {
                    qDebug() << "No free FBOs available after deadline";
                    return std::move(std::shared_ptr<QOpenGLFramebufferObject>{});
                }
            }
        }
        auto fboId = m_pool->fboPool_free.dequeue();
        auto fbo = std::shared_ptr<QOpenGLFramebufferObject>{m_pool->fboPool_all.at(fboId).get(), FBOReturner(m_pool.get(), fboId)};
        m_pool->fboPool_used.emplace(fboId, fbo);
        return fbo;
    }

    std::shared_ptr<QOpenGLFramebufferObject> FBOPool::getFBO(int64_t msecTimeout) {
        return std::move(getFBO(QDeadlineTimer::current() + msecTimeout));
    }

    FBOPoolPrivate::~FBOPoolPrivate() {
        QMutexLocker locker(&poolMutex);
        for (auto &fbo : fboPool_used) {
            if (auto sharedFbo = fbo.second.lock()) {
                std::get_deleter<FBOReturner>(sharedFbo)->m_isDestructing = true;
            }
        }

        fboPool_free.clear();
        fboPool_used.clear();
        fboPool_all.clear();

        fboPool_available.notify_all();
    }

    void FBOReturner::operator()(QOpenGLFramebufferObject *fbo) {
        if (!fbo) {
            qWarning() << "Trying to release a nullptr FBO";
            return;
        }
        if (!m_isDestructing) {
            QMutexLocker locker(&p->poolMutex);

            if (std::find_if(p->fboPool_all.begin(), p->fboPool_all.end(),
                             [this](const std::pair<uint64_t, const std::unique_ptr<QOpenGLFramebufferObject> &> &fbo2) {
                                 return fbo2.first == m_fboId;
                             }) == p->fboPool_all.end()) {
                qWarning() << "Trying to release an FBO that is not in the pool";
                return;
            }

            p->fboPool_used.erase(m_fboId);
            p->fboPool_free.enqueue(m_fboId);
            p->fboPool_available.notify_one();
        }
    }
}// namespace AVQt::common
