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

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QObject>
#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLFramebufferObject>


namespace AVQt::common {
    class FBOPoolPrivate;
    class FBOPool {
    public:
        explicit FBOPool(QSize fboSize, bool dynamic = true, size_t size = 4, size_t maxSize = 8);

        std::shared_ptr<QOpenGLFramebufferObject> getFBO(QDeadlineTimer deadline = QDeadlineTimer::Forever);
        std::shared_ptr<QOpenGLFramebufferObject> getFBO(int64_t msecTimeout);

    protected:
        void allocateNewFBOs(size_t count);

    private:
        std::shared_ptr<FBOPoolPrivate> m_pool{};

        Q_DISABLE_COPY_MOVE(FBOPool)

        friend class FBOPoolPrivate;
    };
}// namespace AVQt::common

#endif//LIBAVQT_FBOPOOL_HPP
