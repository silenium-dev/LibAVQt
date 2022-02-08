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
#include "VAAPIDecoderImpl.hpp"

#include <QDebug>
#include <QProcessEnvironment>
#include <QtConcurrent>

namespace AVQt {
    VideoDecoderFactory &VideoDecoderFactory::getInstance() {
        static VideoDecoderFactory instance;
        return instance;
    }

    bool VideoDecoderFactory::registerDecoder(const api::VideoDecoderInfo &info) {
        for (auto &decoder : m_decoders) {
            if (decoder.name == info.name) {
                qWarning() << "Decoder with name" << info.name << "already registered";
                return false;
            }
        }
        m_decoders.append(info);
        return true;
    }

    void VideoDecoderFactory::unregisterDecoder(const QString &name) {
        QtConcurrent::blockingFilter(m_decoders, [&](const api::VideoDecoderInfo &i) { return i.name != name; });
    }

    void VideoDecoderFactory::unregisterDecoder(const api::VideoDecoderInfo &info) {
        QtConcurrent::blockingFilter(m_decoders, [&](const api::VideoDecoderInfo &i) { return i.name != info.name; });
    }

    std::shared_ptr<api::IVideoDecoderImpl> VideoDecoderFactory::create(const common::PixelFormat &inputFormat, AVCodecID codec, const QStringList &priority) {
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
        QList<api::VideoDecoderInfo> possibleDecoders;

        for (auto &info : m_decoders) {
            if (info.platforms.contains(platform) && info.supportedCodecIds.contains(codec) && info.supportedInputPixelFormats.contains({inputFormat.getCPUFormat(), AV_PIX_FMT_NONE})) {
                possibleDecoders.append(info);
            }
        }

        if (possibleDecoders.isEmpty()) {
            qWarning() << "VideoDecoderFactory: No decoder found for platform";
            return {};
        }

        if (priority.isEmpty()) {
            return std::shared_ptr<api::IVideoDecoderImpl>{qobject_cast<api::IVideoDecoderImpl *>(possibleDecoders.first().metaObject.newInstance(Q_ARG(AVCodecID, codec)))};
        } else {
            for (const auto &decoderName : priority) {
                for (const auto &info : possibleDecoders) {
                    if (info.name == decoderName) {
                        return std::shared_ptr<api::IVideoDecoderImpl>{qobject_cast<api::IVideoDecoderImpl *>(info.metaObject.newInstance(Q_ARG(AVCodecID, codec)))};
                    }
                }
            }
            qWarning() << "VideoDecoderFactory: No decoder found with priority list" << priority;
            qDebug() << "VideoDecoderFactory: Available decoders for platform, codec and input format:";
            for (const auto &info : possibleDecoders) {
                qDebug() << "VideoDecoderFactory: " << info.name;
            }
            return {};
        }
    }

    void VideoDecoderFactory::registerDecoders() {
        static std::atomic_bool registered{false};
        bool shouldBe = false;
        if (registered.compare_exchange_strong(shouldBe, true)) {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
            getInstance().registerDecoder(VAAPIDecoderImpl::info);
#endif
        }
    }
}// namespace AVQt
