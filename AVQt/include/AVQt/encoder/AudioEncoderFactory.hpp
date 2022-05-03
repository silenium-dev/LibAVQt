#ifndef LIBAVQT_AUDIOENCODERFACTORY_HPP
#define LIBAVQT_AUDIOENCODERFACTORY_HPP

#include "AVQt/encoder/IAudioEncoderImpl.hpp"

namespace AVQt {
    class AudioEncoderFactory {
    public:
        static AudioEncoderFactory &getInstance();

        bool registerEncoder(const AudioEncoderInfo &info);

        void unregisterEncoder(const QString &name);
        void unregisterEncoder(const AudioEncoderInfo &info);

        [[nodiscard]] std::shared_ptr<api::IAudioEncoderImpl> create(const common::AudioFormat &inputFormat, AVCodecID codec, const AudioEncodeParameters &encodeParams, const QStringList &priority = {});

    private:
        AudioEncoderFactory() = default;
        QList<AudioEncoderInfo> m_encoders;
    };
}// namespace AVQt

#endif//LIBAVQT_AUDIOENCODERFACTORY_HPP
