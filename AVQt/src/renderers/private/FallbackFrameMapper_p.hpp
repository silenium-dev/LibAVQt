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


#ifndef LIBAVQT_FALLBACKFRAMEMAPPER_P_HPP
#define LIBAVQT_FALLBACKFRAMEMAPPER_P_HPP

#include "AVQt/common/FBOPool.hpp"
#include <QMutex>
#include <QObject>
#include <QQueue>

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

namespace AVQt {
    class FallbackFrameMapper;
    class FallbackFrameMapperPrivate {
        Q_DECLARE_PUBLIC(FallbackFrameMapper)
    public:
        static void destroyOffscreenSurface(QOffscreenSurface *surface);

        static void destroySwsContext(SwsContext *swsContext);

        static void destroyQOpenGLTexture(QOpenGLTexture *texture);

    private:
        explicit FallbackFrameMapperPrivate(FallbackFrameMapper *q) : q_ptr(q) {}

        void bindResources();
        void releaseResources();
        void destroyResources();

        FallbackFrameMapper *q_ptr;

        std::shared_ptr<common::FBOPool> fboPool{};

        QMutex renderMutex{};
        std::unique_ptr<QOffscreenSurface, decltype(&destroyOffscreenSurface)> surface{nullptr, &destroyOffscreenSurface};
        std::unique_ptr<QOpenGLContext> context{};
        std::unique_ptr<QOpenGLTexture, decltype(&destroyQOpenGLTexture)> texture{nullptr, &destroyQOpenGLTexture};

        std::unique_ptr<QOpenGLShaderProgram> program{nullptr};
        QOpenGLVertexArrayObject vao{};
        QOpenGLBuffer vbo{};
        QOpenGLBuffer ibo{};

        QThread *afterStopThread{nullptr};

        std::unique_ptr<SwsContext, decltype(&destroySwsContext)> pSwsContext{nullptr, &destroySwsContext};

        std::atomic_bool paused{false}, firstFrame{true}, running{false};

        static constexpr uint PROGRAM_VERTEX_ATTRIBUTE{0};
        static constexpr uint PROGRAM_TEXCOORD_ATTRIBUTE{1};

        static constexpr uint RENDERQUEUE_MAX_SIZE{2};

        QMutex renderQueueMutex{};
        QQueue<std::shared_ptr<AVFrame>> renderQueue{};

        friend class FallbackFrameMapper;
    };
}// namespace AVQt


#endif//LIBAVQT_FALLBACKFRAMEMAPPER_P_HPP
