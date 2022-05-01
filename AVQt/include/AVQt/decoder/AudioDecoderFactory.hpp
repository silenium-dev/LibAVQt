#ifndef LIBAVQT_AUDIODECODERFACTORY_HPP
#define LIBAVQT_AUDIODECODERFACTORY_HPP

#include "AVQt/decoder/IAudioDecoderImpl.hpp"

#include <list>

namespace AVQt {
    class AudioDecoderFactory {
        Q_DISABLE_COPY_MOVE(AudioDecoderFactory)
    public:
        static AudioDecoderFactory &getInstance();

        bool registerDecoder(const api::AudioDecoderInfo &info);

        void unregisterDecoder(const QString &name);
        void unregisterDecoder(const api::AudioDecoderInfo &info);

        [[nodiscard]] std::shared_ptr<api::IAudioDecoderImpl> create(const common::AudioFormat &inputFormat, AVCodecID codec, const QStringList &priority = {});


    private:
        AudioDecoderFactory() = default;
        std::list<api::AudioDecoderInfo> m_decoders;
    };
}// namespace AVQt

#endif//LIBAVQT_AUDIODECODERFACTORY_HPP
