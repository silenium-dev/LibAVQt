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

#ifndef LIBAVQT_VAAPIOPENGLRENDERERIMPL_HPP
#define LIBAVQT_VAAPIOPENGLRENDERERIMPL_HPP

#include "include/AVQt/renderers/IOpenGLRendererImpl.hpp"

#include <QObject>
#include <QOpenGLContext>

namespace AVQt {
    class VAAPIOpenGLRendererImplPrivate;
    class VAAPIOpenGLRendererImpl : public QObject, public api::IOpenGLRendererImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(VAAPIOpenGLRendererImpl)
        Q_INTERFACES(AVQt::api::IOpenGLRendererImpl)
    public:
        explicit VAAPIOpenGLRendererImpl(QObject *parent = nullptr);
        ~VAAPIOpenGLRendererImpl() override = default;

        void initializeGL(QOpenGLContext *context) override;
        void initializePlatformAPI() override;
        void initializeInterop() override;
        void paintGL() override;

        void update() override;
        void enqueue(AVFrame *frame) override;
        bool isInitialized() const override;

        void bindResources() override;
        void releaseResources() override;
        void destroyResources() override;

        void pause(bool state) override;
        bool isUpdateRequired();
        bool isPaused() const override;
    signals:
        void frameReady(int64_t pts, GLuint textureId[2]) override;
        void frameDropped(int64_t pts) override;

    private:
        void mapFrame();
        VAAPIOpenGLRendererImplPrivate *d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPIOPENGLRENDERERIMPL_HPP
