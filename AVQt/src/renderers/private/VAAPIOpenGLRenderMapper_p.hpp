// Copyright (c) 2021-2022.
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


#ifndef LIBAVQT_VAAPIOPENGLRENDERMAPPER_P_HPP
#define LIBAVQT_VAAPIOPENGLRENDERMAPPER_P_HPP

#include "AVQt/common/FBOPool.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <libdrm/drm_fourcc.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drmcommon.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GL/glu.h>

#include <QOffscreenSurface>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QtConcurrent>
#include <libavutil/hwcontext_vaapi.h>


namespace AVQt {
    class VAAPIOpenGLRenderMapper;
    class VAAPIOpenGLRenderMapperPrivate {
        Q_DECLARE_PUBLIC(VAAPIOpenGLRenderMapper)
    public:
        static void destroyOffscreenSurface(QOffscreenSurface *surface);

    private:
        explicit VAAPIOpenGLRenderMapperPrivate(VAAPIOpenGLRenderMapper *q) : q_ptr(q) {}

        void bindResources();
        void releaseResources();
        void destroyResources();

        VAAPIOpenGLRenderMapper *q_ptr;

        QMutex currentFrameMutex{};
        std::shared_ptr<AVFrame> currentFrame{};

        std::unique_ptr<QOpenGLContext> context{};
        std::unique_ptr<QOffscreenSurface, decltype(&destroyOffscreenSurface)> surface{nullptr, &destroyOffscreenSurface};
        QThread *afterStopThread{nullptr};

        std::unique_ptr<common::FBOPool> fboPool{};

        std::unique_ptr<QOpenGLShaderProgram> program{};
        QOpenGLVertexArrayObject vao{};
        QOpenGLBuffer vbo{};
        QOpenGLBuffer ibo{};
        GLuint textures[2]{};

        QWaitCondition frameAvailable{}, frameProcessed{};
        QMutex renderQueueMutex{};
        QQueue<std::shared_ptr<AVFrame>> renderQueue{};

        std::atomic_bool running{false}, firstFrame{true};

        VADisplay vaDisplay{};
        EGLDisplay eglDisplay{EGL_NO_DISPLAY};
        EGLImage eglImages[2]{};

        static constexpr uint PROGRAM_VERTEX_ATTRIBUTE{0};
        static constexpr uint PROGRAM_TEXCOORD_ATTRIBUTE{1};

        static constexpr uint RENDERQUEUE_MAX_SIZE{8};
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPIOPENGLRENDERMAPPER_P_HPP
