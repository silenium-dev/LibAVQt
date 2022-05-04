#ifndef LIBAVQT_AUDIOOUTPUTFACTORY_HPP
#define LIBAVQT_AUDIOOUTPUTFACTORY_HPP

#include "AVQt/common/PixelFormat.hpp"
#include "AVQt/renderers/IAudioOutputImpl.hpp"

namespace AVQt {
    class AudioOutputFactory {
    public:
        static AudioOutputFactory &getInstance();

        bool registerOutput(const api::AudioOutputImplInfo &info);

        void unregisterOutput(const QString &name);
        void unregisterOutput(const api::AudioOutputImplInfo &info);

        [[nodiscard]] std::shared_ptr<api::IAudioOutputImpl> create(const common::AudioFormat &inputFormat, const QStringList &priority = {});

    private:
        AudioOutputFactory() = default;
        QMap<QString, api::AudioOutputImplInfo> m_outputs;
    };
}// namespace AVQt


#endif//LIBAVQT_AUDIOOUTPUTFACTORY_HPP
