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
// Created by silas on 19.12.21.
//

#ifndef LIBAVQT_FALLBACKFRAMEMAPPER_HPP
#define LIBAVQT_FALLBACKFRAMEMAPPER_HPP

#include "AVQt/renderers/IOpenGLFrameMapper.hpp"
#include "private/FallbackFrameMapper_p.hpp"

#include <QOpenGLFunctions>
#include <QThread>

namespace AVQt {
    class FallbackFrameMapperPrivate;
    class FallbackFrameMapper : public QThread, public api::IOpenGLFrameMapper, protected QOpenGLFunctions {
        Q_OBJECT
        Q_DECLARE_PRIVATE(FallbackFrameMapper)
        Q_INTERFACES(AVQt::api::IOpenGLFrameMapper)
    public:
        static const api::OpenGLFrameMapperInfo &info();
        Q_INVOKABLE explicit FallbackFrameMapper(QObject *parent = nullptr);
        ~FallbackFrameMapper() override;

        void initializeGL(QOpenGLContext *context) override;

        void start() override;
        void stop() override;

        void enqueueFrame(const std::shared_ptr<AVFrame> &frame) override;

    signals:
        void frameReady(qint64 pts, const std::shared_ptr<QOpenGLFramebufferObject> &fbo) override;

    protected:
        void run() override;

    private:
        FallbackFrameMapperPrivate *d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_FALLBACKFRAMEMAPPER_HPP
