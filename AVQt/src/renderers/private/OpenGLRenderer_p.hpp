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
// Created by silas on 16.12.21.
//

#ifndef LIBAVQT_OPENGLRENDERER_P_HPP
#define LIBAVQT_OPENGLRENDERER_P_HPP

#include <QObject>
#include "renderers/IOpenGLRendererImpl.hpp"

namespace AVQt {
    class OpenGLRenderer;
    class OpenGLRendererPrivate {
        Q_DECLARE_PUBLIC(OpenGLRenderer)
    private:
        explicit OpenGLRendererPrivate(OpenGLRenderer *q) : q_ptr(q) {}
        OpenGLRenderer *q_ptr;

        std::atomic_bool running{false};

        api::IOpenGLRendererImpl *impl{nullptr};
    };
}// namespace AVQt


#endif//LIBAVQT_OPENGLRENDERER_P_HPP
