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

namespace AVQt {
    VideoDecoderFactory &VideoDecoderFactory::getInstance() {
        static VideoDecoderFactory instance;
        return instance;
    }

    void VideoDecoderFactory::registerDecoder(const api::VideoDecoderInfo &info) {
        if (m_decoders.contains(info.name)) {
            qWarning() << "VideoDecoderFactory: VideoDecoder with name" << info.name << "already registered";
            return;
        }
        m_decoders.insert(info.name, info);
    }

    void VideoDecoderFactory::unregisterDecoder(const QString &name) {
        if (!m_decoders.contains(name)) {
            qWarning() << "VideoDecoderFactory: VideoDecoder with name" << name << "not registered";
            return;
        }
        m_decoders.remove(name);
    }

    void VideoDecoderFactory::unregisterDecoder(const api::VideoDecoderInfo &info) {
        if (!m_decoders.contains(info.name)) {
            qWarning() << "VideoDecoderFactory: VideoDecoder with name" << info.name << "not registered";
            return;
        }
        m_decoders.remove(info.name);
    }

    std::shared_ptr<api::IVideoDecoderImpl> VideoDecoderFactory::create(const common::PixelFormat &inputFormat, const QString &name) {
        auto infos = m_decoders.values();
        QList<api::VideoDecoderInfo> possibleDecoders;

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        bool isWayland = QProcessEnvironment::systemEnvironment().value("XDG_SESSION_TYPE", "").toLower() == "wayland";
        for (const auto &info : infos) {
            if (info.platforms.contains(isWayland ? common::Platform::Linux_Wayland : common::Platform::Linux_X11)) {
                possibleDecoders.append(info);
            }
        }
#elif defined(Q_OS_ANDROID)
        for (const auto &info : infos) {
            if (info.platforms.contains(common::Platform::Android)) {
                possibleDecoders.append(info);
            }
        }
#elif defined(Q_OS_WIN)
        for (const auto &info : infos) {
            if (info.platforms.contains(common::Platform::Windows)) {
                possibleDecoders.append(info);
            }
        }
#elif defined(Q_OS_MACOS)
        for (const auto &info : infos) {
            if (info.platforms.contains(common::Platform::MacOS)) {
                possibleDecoders.append(info);
            }
        }
#elif defined(Q_OS_IOS)
        for (const auto &info : infos) {
            if (info.platforms.contains(common::Platform::iOS)) {
                possibleDecoders.append(info);
            }
        }
#endif

        if (possibleDecoders.isEmpty()) {
            qWarning() << "VideoDecoderFactory: No decoder found for platform";
            return {};
        }

        std::shared_ptr<api::IVideoDecoderImpl> decoder;
        if (!name.isEmpty() && name != "") {
            for (const auto &info : possibleDecoders) {
                if (info.name == name && info.supportedInputPixelFormats.contains(inputFormat)) {
                    return std::shared_ptr<api::IVideoDecoderImpl>{qobject_cast<api::IVideoDecoderImpl *>(info.metaObject.newInstance())};
                }
            }
            qWarning() << "VideoDecoderFactory: No decoder found with name" << name << "and input format" << inputFormat;
            qWarning() << "VideoDecoderFactory: Available decoders for platform:";
            for (const auto &info : possibleDecoders) {
                qWarning() << "VideoDecoderFactory: " << info.name;
            }
            return {};
        } else {
            for (const auto &info : possibleDecoders) {
                if (info.supportedInputPixelFormats.contains(inputFormat)) {
                    return std::shared_ptr<api::IVideoDecoderImpl>{qobject_cast<api::IVideoDecoderImpl *>(info.metaObject.newInstance())};
                }
            }
            qWarning() << "VideoDecoderFactory: No decoder found with input format" << inputFormat;
            qWarning() << "VideoDecoderFactory: Available decoders for platform:";
            for (const auto &info : possibleDecoders) {
                qWarning() << "VideoDecoderFactory: " << info.name;
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
