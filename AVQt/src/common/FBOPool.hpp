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

#ifndef LIBAVQT_FBOPOOL_HPP
#define LIBAVQT_FBOPOOL_HPP


#include <QOpenGLFramebufferObject>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>


namespace AVQt::common {
    class FBOPool {
    public:
        explicit FBOPool(QSize fboSize, bool dynamic = true, int initialSize = 4);
        FBOPool(const FBOPool &);

        ~FBOPool();

        void operator()(QOpenGLFramebufferObject *fbo);

        void returnFBO(const std::shared_ptr<QOpenGLFramebufferObject> &fbo);
        std::shared_ptr<QOpenGLFramebufferObject> getFBO();

    private:
        void allocateNewFBOs(int count);

        static void destroy(FBOPool *pool);

        std::shared_ptr<std::atomic_size_t> m_instances{};
        std::shared_ptr<std::atomic_bool> m_destroyed{std::make_shared<std::atomic_bool>(false)};
        bool m_dynamic;
        QSize m_fboSize;
        std::shared_ptr<std::mutex> m_poolMutex, m_queueMutex;
        std::shared_ptr<std::set<std::unique_ptr<QOpenGLFramebufferObject>>> m_pool;
        std::shared_ptr<std::queue<std::shared_ptr<QOpenGLFramebufferObject>>> m_queue;
        std::shared_ptr<std::list<std::weak_ptr<QOpenGLFramebufferObject>>> m_usedFBOs;

        std::shared_ptr<std::condition_variable> m_queueCondition;
    };
}// namespace AVQt::common

#endif//LIBAVQT_FBOPOOL_HPP
