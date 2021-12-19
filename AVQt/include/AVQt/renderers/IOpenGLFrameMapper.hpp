// Copyright (c) 2021.
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
// Created by silas on 15.12.21.
//

#ifndef LIBAVQT_IOPENGLFRAMEMAPPER_HPP
#define LIBAVQT_IOPENGLFRAMEMAPPER_HPP

#include <QThread>
#include <QtOpenGL>
extern "C" {
#include <libavutil/frame.h>
}

namespace AVQt::api {
    class IOpenGLFrameMapper {
    public:
        virtual ~IOpenGLFrameMapper() = default;
        virtual void initializeGL(QOpenGLContext *shareContext) = 0;
        virtual void enqueueFrame(AVFrame *frame) = 0;
        virtual void start() = 0;
        virtual void stop() = 0;
    signals:
        virtual void frameReady(qint64 pts, const std::shared_ptr<QOpenGLFramebufferObject> &fbo) = 0;
    };
}// namespace AVQt::api

Q_DECLARE_INTERFACE(AVQt::api::IOpenGLFrameMapper, "AVQt.api.IOpenGLFrameMapper")

#endif//LIBAVQT_IOPENGLFRAMEMAPPER_HPP
