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

#include "FBOPool.hpp"

#include <QtDebug>

namespace AVQt::common {
    FBOPool::FBOPool(QSize fboSize, bool dynamic, int initialSize)
        : m_queue(std::make_shared<std::queue<std::shared_ptr<QOpenGLFramebufferObject>>>()),
          m_pool(std::make_shared<std::set<std::unique_ptr<QOpenGLFramebufferObject>>>()),
          m_usedFBOs(std::make_shared<std::list<std::weak_ptr<QOpenGLFramebufferObject>>>()),
          m_queueMutex(std::make_shared<std::mutex>()),
          m_poolMutex(std::make_shared<std::mutex>()),
          m_queueCondition(std::make_shared<std::condition_variable>()),
          m_fboSize(fboSize),
          m_dynamic(dynamic),
          m_instances(std::make_shared<std::atomic_size_t>(1)) {
        allocateNewFBOs(initialSize);
    }

    FBOPool::FBOPool(const FBOPool &other)
        : m_pool(other.m_pool),
          m_queue(other.m_queue),
          m_usedFBOs(other.m_usedFBOs),
          m_poolMutex(other.m_poolMutex),
          m_queueMutex(other.m_queueMutex),
          m_queueCondition(other.m_queueCondition),
          m_fboSize(other.m_fboSize),
          m_dynamic(other.m_dynamic),
          m_instances(other.m_instances) {
        m_instances->operator++();
    }

    FBOPool::~FBOPool() {
        if (m_instances->load() == 1) {
            bool shouldBe = false;
            if (m_destroyed->compare_exchange_strong(shouldBe, true)) {
                destroy(this);
            }
        }
    }

    void FBOPool::operator()(QOpenGLFramebufferObject *fbo) {
        returnFBO(std::shared_ptr<QOpenGLFramebufferObject>(fbo, *this));
    }

    void FBOPool::returnFBO(const std::shared_ptr<QOpenGLFramebufferObject> &fbo) {
        auto fboInPool = std::find_if(m_pool->begin(), m_pool->end(),
                                      [&fbo](const std::unique_ptr<QOpenGLFramebufferObject> &poolFBO) {
                                          return poolFBO.get() == fbo.get();
                                      });
        if (fboInPool != m_pool->end()) {
            std::unique_lock<std::mutex> queueLock(*m_queueMutex);
            m_usedFBOs->remove_if([&fbo](const std::weak_ptr<QOpenGLFramebufferObject> &usedFBO) {
                return !usedFBO.owner_before(fbo) && !usedFBO.owner_before(fbo);
            });
            m_queue->push(fbo);
            m_queueCondition->notify_one();
        } else {
            qDebug() << "Tried to return FBO that is not in pool!";
        }
    }

    std::shared_ptr<QOpenGLFramebufferObject> FBOPool::getFBO() {
        std::shared_ptr<QOpenGLFramebufferObject> fbo{};
        std::unique_lock<std::mutex> queueLock(*m_queueMutex);// TODO: Fix SEGFAULT on destruction

        if (m_queue->empty() && m_dynamic) {
            allocateNewFBOs(2);
        } else if (m_queue->empty()) {
            m_queueCondition->wait(queueLock);
            if (m_queue->empty()) {
                return fbo;
            }
        }

        fbo = std::move(m_queue->front());
        m_queue->pop();
        m_usedFBOs->emplace_back(fbo);

        return fbo;
    }
    void FBOPool::allocateNewFBOs(int count) {
        std::unique_lock<std::mutex> poolLock(*m_poolMutex);
        for (int i = 0; i < count; i++) {
            auto fbo = std::make_unique<QOpenGLFramebufferObject>(m_fboSize, QOpenGLFramebufferObject::CombinedDepthStencil);
            m_queue->emplace(std::shared_ptr<QOpenGLFramebufferObject>(fbo.get(), *this));
            m_pool->emplace(std::move(fbo));
        }
    }

    void FBOPool::destroy(FBOPool *pool) {
        std::unique_lock<std::mutex> poolLock(*pool->m_poolMutex);
        std::unique_lock<std::mutex> queueLock(*pool->m_queueMutex);
        pool->m_pool->clear();
        pool->m_usedFBOs->clear();
        while (!pool->m_queue->empty()) {
            pool->m_queue->pop();
        }
        pool->m_queueCondition->notify_all();
    }
}// namespace AVQt::common
