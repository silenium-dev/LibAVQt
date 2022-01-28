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

//
// Created by silas on 12.12.21.
//

#include "include/AVQt/decoder/VideoDecoderFactory.hpp"
#include "AVQt/decoder/VAAPIDecoderImpl.hpp"

#include <QDebug>

namespace AVQt {
    VideoDecoderFactory &VideoDecoderFactory::getInstance() {
        static VideoDecoderFactory instance;
        return instance;
    }

    void VideoDecoderFactory::registerDecoder(const QString &name, const QMetaObject &metaObject) {
        if (m_decoders.contains(name)) {
            qWarning() << "VideoDecoderFactory: VideoDecoder with name" << name << "already registered";
            return;
        }
        m_decoders.insert(name, metaObject);
    }

    void VideoDecoderFactory::unregisterDecoder(const QString &name) {
        if (!m_decoders.contains(name)) {
            qWarning() << "VideoDecoderFactory: VideoDecoder with name" << name << "not registered";
            return;
        }
        m_decoders.remove(name);
    }

    std::shared_ptr<api::IVideoDecoderImpl> VideoDecoderFactory::create(const QString &name) {
        if (!m_decoders.contains(name)) {
            qWarning() << "VideoDecoderFactory: VideoDecoder with name" << name << "not registered";
            return nullptr;
        }
        return std::shared_ptr<api::IVideoDecoderImpl>{qobject_cast<api::IVideoDecoderImpl *>(m_decoders[name].newInstance())};
    }

    void VideoDecoderFactory::registerDecoders() {
        static std::atomic_bool registered{false};
        bool shouldBe = false;
        if (registered.compare_exchange_strong(shouldBe, true)) {
#ifdef Q_OS_LINUX
            getInstance().registerDecoder("VAAPI", VAAPIDecoderImpl::staticMetaObject);
#endif
        }
    }
}// namespace AVQt
