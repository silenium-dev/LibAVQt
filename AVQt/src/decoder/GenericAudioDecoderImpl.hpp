#ifndef LIBAVQT_GENERICAUDIODECODERIMPL_HPP
#define LIBAVQT_GENERICAUDIODECODERIMPL_HPP

#include "decoder/IAudioDecoderImpl.hpp"


#include <QObject>

namespace AVQt {
    class GenericAudioDecoderImplPrivate;
    class GenericAudioDecoderImpl : public QObject, public api::IAudioDecoderImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(GenericAudioDecoderImpl)
        Q_DISABLE_COPY_MOVE(GenericAudioDecoderImpl)
        Q_INTERFACES(AVQt::api::IAudioDecoderImpl)
    public:
        static const api::AudioDecoderInfo info();

        Q_INVOKABLE explicit GenericAudioDecoderImpl(AVCodecID codecId, QObject *parent = nullptr);
        ~GenericAudioDecoderImpl() Q_DECL_OVERRIDE;

        bool open(std::shared_ptr<AVCodecParameters> codecParams) Q_DECL_OVERRIDE;
        void close() Q_DECL_OVERRIDE;

        virtual int decode(std::shared_ptr<AVPacket> packet) Q_DECL_OVERRIDE;

        [[nodiscard]] common::AudioFormat getOutputFormat() const Q_DECL_OVERRIDE;
        [[nodiscard]] communication::AudioPadParams getAudioParams() const Q_DECL_OVERRIDE;

    signals:
        void frameReady(std::shared_ptr<AVFrame> frame) Q_DECL_OVERRIDE;

    private:
        std::unique_ptr<GenericAudioDecoderImplPrivate> d_ptr;
    };
}// namespace AVQt

#endif//LIBAVQT_GENERICAUDIODECODERIMPL_HPP
