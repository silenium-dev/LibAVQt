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

#ifndef LIBAVQT_VAAPIOPENGLRENDERMAPPER_HPP
#define LIBAVQT_VAAPIOPENGLRENDERMAPPER_HPP

#include "include/AVQt/renderers/IOpenGLFrameMapper.hpp"

#include <QObject>
#include <QOpenGLContext>

namespace AVQt {
    class VAAPIOpenGLRenderMapperPrivate;
    class VAAPIOpenGLRenderMapper : public QThread, public api::IOpenGLFrameMapper, protected QOpenGLFunctions {
        Q_OBJECT
        Q_DECLARE_PRIVATE(VAAPIOpenGLRenderMapper)
        Q_INTERFACES(AVQt::api::IOpenGLFrameMapper)
    public:
        Q_INVOKABLE explicit VAAPIOpenGLRenderMapper(QObject *parent = nullptr);
        ~VAAPIOpenGLRenderMapper() Q_DECL_OVERRIDE;

        void initializeGL(QOpenGLContext *shareContext) Q_DECL_OVERRIDE Q_DECL_UNUSED;
        void enqueueFrame(const std::shared_ptr<AVFrame> &frame) Q_DECL_OVERRIDE Q_DECL_UNUSED;

        void start() Q_DECL_OVERRIDE Q_DECL_UNUSED;
        void stop() Q_DECL_OVERRIDE Q_DECL_UNUSED;
    signals:
        void frameReady(qint64 pts, const std::shared_ptr<QOpenGLFramebufferObject> &fbo) Q_DECL_OVERRIDE;

    protected:
        void run() Q_DECL_OVERRIDE;

    private:
        void initializePlatformAPI();
        void initializeInterop();
        void mapFrame();
        std::unique_ptr<VAAPIOpenGLRenderMapperPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPIOPENGLRENDERMAPPER_HPP
