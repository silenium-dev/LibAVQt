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

#ifndef LIBAVQT_DRM_OPENGL_RENDERMAPPER_P_HPP
#define LIBAVQT_DRM_OPENGL_RENDERMAPPER_P_HPP

#include "common/FBOPool.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <QObject>
#include <QtOpenGL>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
}

namespace AVQt {
    class DRM_OpenGL_RenderMapper;
    class DRM_OpenGL_RenderMapperPrivate {
        Q_DECLARE_PUBLIC(DRM_OpenGL_RenderMapper)
    private:
        explicit DRM_OpenGL_RenderMapperPrivate(DRM_OpenGL_RenderMapper *q) : q_ptr(q) {}

        void bindResources();
        void releaseResources();
        void destroyResources();

        DRM_OpenGL_RenderMapper *q_ptr;

        std::shared_ptr<common::FBOPool> fboPool{};

        std::unique_ptr<QOffscreenSurface> surface{};
        std::unique_ptr<QOpenGLContext> context{};

        std::unique_ptr<QOpenGLShaderProgram> program{};
        std::unique_ptr<QOpenGLVertexArrayObject> vao{};
        std::unique_ptr<QOpenGLBuffer> vbo{};
        std::unique_ptr<QOpenGLBuffer> ibo{};

        QMutex currentFrameMutex{};
        std::shared_ptr<AVFrame> currentFrame{};

        QMutex inputQueueMutex{};
        QQueue<std::shared_ptr<AVFrame>> inputQueue{};

        QWaitCondition frameProcessed{}, frameAvailable{};

        QThread *afterStopThread{nullptr};

        static constexpr uint PROGRAM_VERTEX_ATTRIBUTE{0};
        static constexpr uint PROGRAM_TEXCOORD_ATTRIBUTE{1};

        EGLDisplay eglDisplay{EGL_NO_DISPLAY};
        EGLImageKHR eglImages[2]{EGL_NO_IMAGE_KHR, EGL_NO_IMAGE_KHR};
        GLuint textures[2]{0, 0};

        std::atomic_bool running{false}, paused{false}, firstFrame{true};
    };
}// namespace AVQt


#endif//LIBAVQT_DRM_OPENGL_RENDERMAPPER_P_HPP
