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

#ifndef LIBAVQT_DRM_OPENGL_RENDERMAPPER_HPP
#define LIBAVQT_DRM_OPENGL_RENDERMAPPER_HPP

#include "renderers/IOpenGLFrameMapper.hpp"

#include <QtCore>

namespace AVQt {
    class DRM_OpenGL_RenderMapperPrivate;

    class DRM_OpenGL_RenderMapper : public QObject, public api::IOpenGLFrameMapper {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IOpenGLFrameMapper)
        Q_DECLARE_PRIVATE(DRM_OpenGL_RenderMapper)
    public:
        explicit DRM_OpenGL_RenderMapper(QObject *parent = nullptr);
        ~DRM_OpenGL_RenderMapper() override;
        //
        //        void initializeGL(QOpenGLContext *shareContext) override;
        //
        //        void start() override;
        //        void stop() override;
        //
        //        void enqueueFrame(AVFrame *frame) override;
        //    signals:
        //        void frameReady(qint64 pts, const std::shared_ptr<QOpenGLFramebufferObject> &fbo) override;

    private:
        QScopedPointer<DRM_OpenGL_RenderMapperPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_DRM_OPENGL_RENDERMAPPER_HPP
