#ifndef LIBAVQT_GENERICAUDIOENCODERIMPL_HPP
#define LIBAVQT_GENERICAUDIOENCODERIMPL_HPP

#include "encoder/IAudioEncoderImpl.hpp"

#include <QObject>

namespace AVQt {
    class GenericAudioEncoderImplPrivate;
    class GenericAudioEncoderImpl : public QObject, public api::IAudioEncoderImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(GenericAudioEncoderImpl)
        Q_INTERFACES(AVQt::api::IAudioEncoderImpl)
    public:
        static const AudioEncoderInfo &info();

        Q_INVOKABLE explicit GenericAudioEncoderImpl(AVCodecID codecId, AVQt::AudioEncodeParameters encodeParameters, QObject *parent = nullptr);
        ~GenericAudioEncoderImpl() override;

        bool open(const communication::AudioPadParams &params) override;
        void close() override;
        int encode(std::shared_ptr<AVFrame> frame) override;
        [[nodiscard]] QVector<AVSampleFormat> getInputFormats() const override;
        [[nodiscard]] std::shared_ptr<AVCodecParameters> getCodecParameters() const override;
        [[nodiscard]] std::shared_ptr<communication::PacketPadParams> getPacketPadParams() const override;

    signals:
        void packetReady(std::shared_ptr<AVPacket> packet) override;

    private:
        std::shared_ptr<GenericAudioEncoderImplPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_GENERICAUDIOENCODERIMPL_HPP
