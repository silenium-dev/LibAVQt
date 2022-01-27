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

#ifndef LIBAVQT_OPENGLFRAMEMAPPERFACTORY_HPP
#define LIBAVQT_OPENGLFRAMEMAPPERFACTORY_HPP

#include "IOpenGLFrameMapper.hpp"

#include <QMap>
#include <QObject>
#include <static_block.hpp>

namespace AVQt {
    class OpenGLFrameMapperFactory {
    public:
        static OpenGLFrameMapperFactory &getInstance();

        void registerRenderer(const QString &name, const QMetaObject &metaObject);

        void unregisterRenderer(const QString &name);

        [[nodiscard]] std::shared_ptr<api::IOpenGLFrameMapper> create(const QString &name);

        static void registerDecoder();

    private:
        OpenGLFrameMapperFactory() = default;
        QMap<QString, QMetaObject> m_renderers;
    };
}// namespace AVQt

static_block {
    AVQt::OpenGLFrameMapperFactory::registerDecoder();
}

#endif//LIBAVQT_OPENGLFRAMEMAPPERFACTORY_HPP
