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

#include "AVQt/renderers/OpenGLFrameMapperFactory.hpp"

namespace AVQt {
    OpenGLFrameMapperFactory &OpenGLFrameMapperFactory::getInstance() {
        static OpenGLFrameMapperFactory instance;
        return instance;
    }

    bool OpenGLFrameMapperFactory::registerRenderer(const api::OpenGLFrameMapperInfo &info) {
        for (const auto &i : m_renderers) {
            if (i.name == info.name) {
                qWarning() << "OpenGLFrameMapperFactory::registerRenderer: Renderer with name " << info.name << " already registered";
                return false;
            }
        }
        if (info.isSupported()) {
            m_renderers.insert(info.name, info);
            return true;
        }
        return false;
    }

    void OpenGLFrameMapperFactory::unregisterRenderer(const QString &name) {
        m_renderers.remove(name);
    }

    void OpenGLFrameMapperFactory::unregisterRenderer(const api::OpenGLFrameMapperInfo &info) {
        m_renderers.remove(info.name);
    }

    std::shared_ptr<api::IOpenGLFrameMapper> OpenGLFrameMapperFactory::create(const common::PixelFormat &inputFormat, const QStringList &priority) {
        QList<api::OpenGLFrameMapperInfo> possibleRenderers;
        if (priority.isEmpty()) {
            for (const auto &info : m_renderers) {
                if (common::Platform::isAvailable(info.platforms) &&
                    inputFormat.isSupportedBy(info.supportedInputPixelFormats)) {
                    possibleRenderers.append(info);
                }
            }
        } else {
            for (const auto &prio : priority) {
                if (m_renderers.contains(prio)) {
                    auto info = m_renderers[prio];
                    if (common::Platform::isAvailable(info.platforms) &&
                        inputFormat.isSupportedBy(info.supportedInputPixelFormats)) {
                        possibleRenderers.append(info);
                    }
                }
            }
        }
        if (possibleRenderers.isEmpty()) {
            return {};
        }
        auto obj = possibleRenderers.first().metaObject.newInstance();
        if (obj) {
            auto renderer = qobject_cast<api::IOpenGLFrameMapper *>(obj);
            if (renderer) {
                return std::shared_ptr<api::IOpenGLFrameMapper>(renderer);
            }
        }
        return {};
    }
}// namespace AVQt
