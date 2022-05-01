#include "decoder/AudioDecoderFactory.hpp"

#include <QObject>
#include <QProcessEnvironment>

namespace AVQt {
    AudioDecoderFactory &AudioDecoderFactory::getInstance() {
        static AudioDecoderFactory instance;
        return instance;
    }

    bool AudioDecoderFactory::registerDecoder(const api::AudioDecoderInfo &info) {
        if (std::find_if(m_decoders.begin(), m_decoders.end(), [&](const api::AudioDecoderInfo &li) { return li.name == info.name; }) != m_decoders.end()) {
            return false;
        }

        m_decoders.emplace_back(info);
        return true;
    }

    void AudioDecoderFactory::unregisterDecoder(const QString &name) {
        auto it = std::find_if(m_decoders.begin(), m_decoders.end(), [&](const api::AudioDecoderInfo &li) { return li.name == name; });
        if (it != m_decoders.end()) {
            m_decoders.erase(it);
        }
    }

    void AudioDecoderFactory::unregisterDecoder(const api::AudioDecoderInfo &info) {
        auto it = std::find_if(m_decoders.begin(), m_decoders.end(), [&](const api::AudioDecoderInfo &li) { return li.name == info.name; });
        if (it != m_decoders.end()) {
            m_decoders.erase(it);
        }
    }

    std::shared_ptr<api::IAudioDecoderImpl> AudioDecoderFactory::create(const common::AudioFormat &inputFormat, AVCodecID codec, const QStringList &priority) {
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
#else
#error "Unsupported platform"
#endif
        QList<api::AudioDecoderInfo> possibleDecoders;

        for (auto &info : m_decoders) {
            if ((info.platforms.contains(common::Platform::All) || info.platforms.contains(platform)) &&
                (info.supportedCodecIds.empty() || info.supportedCodecIds.contains(codec)) &&
                (info.supportedInputAudioFormats.empty() || std::find_if(info.supportedInputAudioFormats.begin(), info.supportedInputAudioFormats.end(),
                                                                         [&](const common::AudioFormat &item) {
                                                                             return item == inputFormat;
                                                                         }) != info.supportedInputAudioFormats.end())) {
                possibleDecoders.append(info);
            }
        }

        if (possibleDecoders.isEmpty()) {
            qWarning() << "AudioDecoderFactory: No decoder found for platform";
            return {};
        }

        if (priority.isEmpty()) {
            auto qobj = possibleDecoders.first().metaObject.newInstance(Q_ARG(AVCodecID, codec));
            auto inst = qobject_cast<api::IAudioDecoderImpl *>(qobj);
            return std::shared_ptr<api::IAudioDecoderImpl>{inst};
        } else {
            for (const auto &decoderName : priority) {
                for (const auto &info : possibleDecoders) {
                    if (info.name == decoderName) {
                        auto qobj = info.metaObject.newInstance(Q_ARG(AVCodecID, codec));
                        auto inst = qobject_cast<api::IAudioDecoderImpl *>(qobj);
                        return std::shared_ptr<api::IAudioDecoderImpl>{inst};
                    }
                }
            }
            qWarning() << "AudioDecoderFactory: No decoder found with priority list" << priority;
            qDebug() << "AudioDecoderFactory: Available decoders for platform, codec and input format:";
            for (const auto &info : possibleDecoders) {
                qDebug() << "AudioDecoderFactory: " << info.name;
            }
            return {};
        }
    }
}// namespace AVQt
