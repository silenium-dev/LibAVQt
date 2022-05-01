#ifndef LIBAVQT_GENERICAUDIODECODERIMPLPRIVATE_HPP
#define LIBAVQT_GENERICAUDIODECODERIMPLPRIVATE_HPP

#include <QObject>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace AVQt {
    class GenericAudioDecoderImpl;
    class GenericAudioDecoderImplPrivate {
        Q_DISABLE_COPY_MOVE(GenericAudioDecoderImplPrivate)
        Q_DECLARE_PUBLIC(GenericAudioDecoderImpl)
    public:
        static void destroyAVCodecContext(AVCodecContext *codecContext);
        static void destroyAVFrame(AVFrame *frame);

    private:
        explicit GenericAudioDecoderImplPrivate(GenericAudioDecoderImpl *q);
        GenericAudioDecoderImpl *q_ptr;

        void init(AVCodecID codecId);

        const AVCodec *codec{nullptr};
        std::unique_ptr<AVCodecContext, decltype(&destroyAVCodecContext)> codecContext{nullptr, destroyAVCodecContext};
        std::shared_ptr<AVCodecParameters> codecParameters{};

        std::atomic_bool open{false};
    };
}// namespace AVQt

#endif//LIBAVQT_GENERICAUDIODECODERIMPLPRIVATE_HPP
