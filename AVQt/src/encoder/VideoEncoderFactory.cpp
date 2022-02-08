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
#include <QtConcurrent>

namespace AVQt {
    VideoEncoderFactory &VideoEncoderFactory::getInstance() {
        static VideoEncoderFactory instance;
        return instance;
    }

    bool VideoEncoderFactory::registerEncoder(const api::VideoEncoderInfo &info) {
        for (auto &encoder : m_encoders) {
            if (encoder.name == info.name) {
                qWarning() << "Encoder" << info.name << "already registered";
                return false;
            }
        }
        m_encoders.append(info);
        return true;
    }

    void VideoEncoderFactory::unregisterEncoder(const QString &name) {
        QtConcurrent::blockingFilter(m_encoders, [&](const api::VideoEncoderInfo &i) { return i.name != name; });
    }

    void VideoEncoderFactory::unregisterEncoder(const api::VideoEncoderInfo &info) {
        QtConcurrent::blockingFilter(m_encoders, [&](const api::VideoEncoderInfo &i) { return i.name != info.name; });
    }

    std::shared_ptr<api::IVideoEncoderImpl> VideoEncoderFactory::create(const common::PixelFormat &inputFormat, AVCodecID codec, const EncodeParameters &encodeParams, const QStringList &priority) {
        auto platform = common::Platform::Unknown;
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        bool isWayland = QProcessEnvironment::systemEnvironment().value("XDG_SESSION_TYPE", "").toLower() == "wayland";
        platform = isWayland ? common::Platform::Linux_Wayland : common::Platform::Linux_X11;
#elif defined(Q_OS_ANDROID)
        platform = common::Platform::Android;
#elif defined(Q_OS_WIN)
        platform = common::Platform::Windows;
#elif defined(Q_OS_MACOS)
        platform = common::Platform::MacOS;
#elif defined(Q_OS_IOS)
        platform = common::Platform::iOS;
#endif

        QList<api::VideoEncoderInfo> possibleEncoders;
        for (auto &encoder : m_encoders) {
            if (encoder.platforms.contains(platform) && encoder.supportedInputPixelFormats.contains(inputFormat) && encoder.supportedCodecIds.contains(codec)) {
                possibleEncoders.append(encoder);
            }
        }

        if (possibleEncoders.isEmpty()) {
            qWarning() << "VideoEncoderFactory: No encoder found for" << avcodec_get_name(codec) << "with" << inputFormat.toString();
            return {};
        }

        if (priority.isEmpty()) {
            return std::shared_ptr<api::IVideoEncoderImpl>(
                    qobject_cast<api::IVideoEncoderImpl *>(
                            possibleEncoders.first().metaObject.newInstance(Q_ARG(AVCodecID, codec),
                                                                            Q_ARG(EncodeParameters, encodeParams))));
        } else {
            for (const auto &encoderName : priority) {
                for (const auto &info : possibleEncoders) {
                    if (info.name == encoderName) {
                        return std::shared_ptr<api::IVideoEncoderImpl>(
                                qobject_cast<api::IVideoEncoderImpl *>(
                                        info.metaObject.newInstance(Q_ARG(AVCodecID, codec),
                                                                    Q_ARG(EncodeParameters, encodeParams))));
                    }
                }
            }
            qWarning() << "VideoEncoderFactory: No encoder found for" << priority << "with" << inputFormat << "and" << avcodec_get_name(codec);
            qDebug() << "VideoEncoderFactory: Available encoders:";
            for (auto &encoder : possibleEncoders) {
                qDebug() << "VideoEncoderFactory:" << encoder.name;
            }
            return {};
        }
    }

    void VideoEncoderFactory::registerEncoders() {
        static std::atomic_bool registered{false};
        bool shouldBe = false;
        if (registered.compare_exchange_strong(shouldBe, true)) {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
            getInstance().registerEncoder(VAAPIEncoderImpl::info);
#endif
        }
    }
}// namespace AVQt
