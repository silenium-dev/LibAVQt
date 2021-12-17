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
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BELIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORTOR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OROTHER DEALINGS IN THE SOFTWARE.

//
// Created by silas on 15.12.21.
//

#ifndef LIBAVQT_IOPENGLRENDERERIMPL_HPP
#define LIBAVQT_IOPENGLRENDERERIMPL_HPP

#include <QObject>
#include <QOpenGLContext>
extern "C" {
#include <libavutil/frame.h>
}

namespace AVQt::api {
    class IOpenGLRendererImpl {
    public:
        virtual ~IOpenGLRendererImpl() = default;
        virtual void initializeGL(QOpenGLContext *context) = 0;
        virtual void initializePlatformAPI() = 0;
        virtual void initializeInterop() = 0;
        virtual void paintGL() = 0;

        virtual void update() = 0;
        virtual void enqueue(AVFrame *frame) = 0;
        [[nodiscard]] virtual bool isInitialized() const = 0;

        virtual void bindResources() = 0;
        virtual void releaseResources() = 0;
        virtual void destroyResources() = 0;

        virtual void pause(bool state) = 0;
        [[nodiscard]] virtual bool isPaused() const = 0;
    signals:
        virtual void frameReady(int64_t pts, GLuint textureId[2]) = 0;
        virtual void frameDropped(int64_t pts) = 0;
    };
}// namespace AVQt::api

Q_DECLARE_INTERFACE(AVQt::api::IOpenGLRendererImpl, "AVQt.api.IOpenGLRendererImpl")

#endif//LIBAVQT_IOPENGLRENDERERIMPL_HPP
