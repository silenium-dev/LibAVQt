#include "renderers/AudioOutputFactory.hpp"

namespace AVQt {
    AudioOutputFactory &AudioOutputFactory::getInstance() {
        static AudioOutputFactory instance;
        return instance;
    }

    bool AudioOutputFactory::registerOutput(const api::AudioOutputImplInfo &info) {
        if (info.name.isEmpty()) {
            qWarning() << "AudioOutputFactory: registerOutput: name is empty";
            return false;
        }

        if (m_outputs.contains(info.name)) {
            qWarning() << "AudioOutputFactory: Output" << info.name << "already registered";
            return false;
        }

        m_outputs.insert(info.name, info);
        return true;
    }

    void AudioOutputFactory::unregisterOutput(const QString &name) {
        m_outputs.remove(name);
    }

    void AudioOutputFactory::unregisterOutput(const api::AudioOutputImplInfo &info) {
        m_outputs.remove(info.name);
    }

    std::shared_ptr<api::IAudioOutputImpl> AudioOutputFactory::create(const common::AudioFormat &inputFormat, const QStringList &priority) {
        QList<api::AudioOutputImplInfo> possibleRenderers;
        if (priority.isEmpty()) {
            for (const auto &info : m_outputs) {
                if (common::Platform::isAvailable(info.supportedPlatforms) &&
                    inputFormat.isSupportedBy(info.supportedFormats)) {
                    possibleRenderers.append(info);
                }
            }
        } else {
            for (const auto &prio : priority) {
                if (m_outputs.contains(prio)) {
                    auto info = m_outputs[prio];
                    if (common::Platform::isAvailable(info.supportedPlatforms) &&
                        inputFormat.isSupportedBy(info.supportedFormats)) {
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
            auto renderer = qobject_cast<api::IAudioOutputImpl *>(obj);
            if (renderer) {
                return std::shared_ptr<api::IAudioOutputImpl>(renderer);
            }
        }
        return {};
    }
}// namespace AVQt
