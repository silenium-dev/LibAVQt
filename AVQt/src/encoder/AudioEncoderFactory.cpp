#include "encoder/AudioEncoderFactory.hpp"

#include <QtConcurrent>

namespace AVQt {
    AudioEncoderFactory &AudioEncoderFactory::getInstance() {
        static AudioEncoderFactory instance;
        return instance;
    }

    bool AudioEncoderFactory::registerEncoder(const AudioEncoderInfo &info) {
        for (auto &encoder : m_encoders) {
            if (encoder.name == info.name) {
                qWarning() << "Encoder" << info.name << "already registered";
                return false;
            }
        }
        m_encoders.append(info);
        return true;
    }

    void AudioEncoderFactory::unregisterEncoder(const QString &name) {
        QtConcurrent::blockingFilter(m_encoders, [&](const AudioEncoderInfo &i) { return i.name != name; });
    }

    void AudioEncoderFactory::unregisterEncoder(const AudioEncoderInfo &info) {
        QtConcurrent::blockingFilter(m_encoders, [&](const AudioEncoderInfo &i) { return i.name != info.name; });
    }

    std::shared_ptr<api::IAudioEncoderImpl> AudioEncoderFactory::create(const common::AudioFormat &inputFormat, AVCodecID codec, const AudioEncodeParameters &encodeParams, const QStringList &priority) {
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

        QList<AudioEncoderInfo> possibleEncoders;
        for (auto &encoder : m_encoders) {
            if ((encoder.platforms.contains(common::Platform::All) || encoder.platforms.contains(platform)) &&
                (encoder.supportedInputFormats.isEmpty() || encoder.supportedInputFormats.contains(inputFormat)) &&
                (encoder.supportedCodecIds.isEmpty() || encoder.supportedCodecIds.contains(codec))) {
                possibleEncoders.append(encoder);
            }
        }

        if (possibleEncoders.isEmpty()) {
            qWarning() << "AudioEncoderFactory: No encoder found for" << avcodec_get_name(codec) << "with" << inputFormat.toString();
            return {};
        }

        if (priority.isEmpty()) {
            auto inst = possibleEncoders.first().metaObject.newInstance(Q_ARG(AVCodecID, codec),
                                                                        Q_ARG(AVQt::AudioEncodeParameters, encodeParams));
            return std::shared_ptr<api::IAudioEncoderImpl>(qobject_cast<api::IAudioEncoderImpl *>(inst));
        } else {
            for (const auto &encoderName : priority) {
                for (const auto &info : possibleEncoders) {
                    if (info.name == encoderName) {
                        return std::shared_ptr<api::IAudioEncoderImpl>(
                                qobject_cast<api::IAudioEncoderImpl *>(
                                        info.metaObject.newInstance(Q_ARG(AVCodecID, codec),
                                                                    Q_ARG(AVQt::AudioEncodeParameters, encodeParams))));
                    }
                }
            }
            qWarning() << "AudioEncoderFactory: No encoder found for" << priority << "with" << inputFormat.toString() << "and" << avcodec_get_name(codec);
            qDebug() << "AudioEncoderFactory: Available encoders:";
            for (auto &encoder : possibleEncoders) {
                qDebug() << "AudioEncoderFactory:" << encoder.name;
            }
            return {};
        }
    }
}// namespace AVQt
