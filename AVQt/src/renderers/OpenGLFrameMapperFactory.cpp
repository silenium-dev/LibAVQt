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

#include "renderers/OpenGLFrameMapperFactory.hpp"
#include "renderers/FallbackFrameMapper.hpp"
#include "renderers/VAAPIOpenGLRenderMapper.hpp"

namespace AVQt {
    OpenGLFrameMapperFactory &OpenGLFrameMapperFactory::getInstance() {
        static OpenGLFrameMapperFactory instance;
        return instance;
    }

    void OpenGLFrameMapperFactory::registerRenderer(const QString &name, const QMetaObject &metaObject) {
        m_renderers.insert(name, metaObject);
    }

    void OpenGLFrameMapperFactory::unregisterRenderer(const QString &name) {
        m_renderers.remove(name);
    }

    api::IOpenGLFrameMapper *OpenGLFrameMapperFactory::create(const QString &name) {
        return qobject_cast<api::IOpenGLFrameMapper *>(m_renderers[name].newInstance());
    }

    void OpenGLFrameMapperFactory::registerDecoder() {
        static bool registered = false;
        if (!registered) {
            registered = true;
            AVQt::OpenGLFrameMapperFactory::getInstance().registerRenderer("VAAPIOpenGLRenderMapper", VAAPIOpenGLRenderMapper::staticMetaObject);
            AVQt::OpenGLFrameMapperFactory::getInstance().registerRenderer("FallbackMapper", FallbackFrameMapper::staticMetaObject);
        }
    }
}// namespace AVQt
