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
// Created by silas on 28.12.21.
//

#include "AVQt/encoder/VideoEncoderFactory.hpp"
#include "VAAPIEncoderImpl.hpp"

#include <QtDebug>

namespace AVQt {
    VideoEncoderFactory &VideoEncoderFactory::getInstance() {
        static VideoEncoderFactory instance;
        return instance;
    }

    void VideoEncoderFactory::registerEncoder(const QString &name, const QMetaObject &metaObject) {
        if (m_encoders.contains(name)) {
            qWarning() << "VideoEncoderFactory: VideoEncoder with name" << name << "already registered";
            return;
        }
        m_encoders.insert(name, metaObject);
    }

    void VideoEncoderFactory::unregisterEncoder(const QString &name) {
        if (!m_encoders.contains(name)) {
            qWarning() << "VideoEncoderFactory: VideoEncoder with name" << name << "not registered";
            return;
        }
        m_encoders.remove(name);
    }

    std::shared_ptr<api::IVideoEncoderImpl> VideoEncoderFactory::create(const QString &name, const EncodeParameters &params) {
        if (!m_encoders.contains(name)) {
            qWarning() << "VideoEncoderFactory: VideoEncoder with name" << name << "not registered";
            return nullptr;
        }
        return std::shared_ptr<api::IVideoEncoderImpl>{qobject_cast<api::IVideoEncoderImpl *>(m_encoders[name].newInstance(Q_ARG(AVQt::EncodeParameters, params)))};
    }

    void VideoEncoderFactory::registerEncoders() {
        static std::atomic_bool registered{false};
        bool shouldBe = false;
        if (registered.compare_exchange_strong(shouldBe, true)) {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
            getInstance().registerEncoder("VAAPI", VAAPIEncoderImpl::staticMetaObject);
#endif
        }
    }
}// namespace AVQt
