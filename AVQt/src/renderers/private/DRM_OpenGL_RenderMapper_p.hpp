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

#include <QObject>

namespace AVQt {
    class DRM_OpenGL_RenderMapper;
    class DRM_OpenGL_RenderMapperPrivate {
        Q_DECLARE_PUBLIC(DRM_OpenGL_RenderMapper)
    private:
        explicit DRM_OpenGL_RenderMapperPrivate(DRM_OpenGL_RenderMapper *q) : q_ptr(q) {}

        DRM_OpenGL_RenderMapper *q_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_DRM_OPENGL_RENDERMAPPER_P_HPP
