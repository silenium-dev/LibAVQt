#ifndef LIBAVQT_GENERICAUDIOENCODERIMPL_P_HPP
#define LIBAVQT_GENERICAUDIOENCODERIMPL_P_HPP

#include "encoder/IAudioEncoderImpl.hpp"

#include <QObject>

namespace AVQt {
    class GenericAudioEncoderImpl;
    class GenericAudioEncoderImplPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(GenericAudioEncoderImpl)
    public:
        static void destroyAVCodecContext(AVCodecContext *codecContext);

        static void destroyAVCodecParameters(AVCodecParameters *codecParameters);

    private:
        explicit GenericAudioEncoderImplPrivate(AVCodecID codecId, AudioEncodeParameters encodeParameters, GenericAudioEncoderImpl *q);
        GenericAudioEncoderImpl *q_ptr;

        AudioEncodeParameters encodeParameters{};
        communication::AudioPadParams inputParameters{};

        const AVCodec *codec;
        std::unique_ptr<AVCodecContext, decltype(&destroyAVCodecContext)> codecContext{nullptr, &destroyAVCodecContext};

        std::atomic_bool open{false};
    };
}// namespace AVQt


#endif//LIBAVQT_GENERICAUDIOENCODERIMPL_P_HPP
