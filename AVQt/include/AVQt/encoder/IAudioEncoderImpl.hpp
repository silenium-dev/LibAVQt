#ifndef LIBAVQT_IAUDIOENCODERIMPL_HPP
#define LIBAVQT_IAUDIOENCODERIMPL_HPP

#include "AVQt/common/AudioFormat.hpp"
#include "AVQt/common/Platform.hpp"
#include "AVQt/communication/AudioPadParams.hpp"
#include "AVQt/communication/PacketPadParams.hpp"

#include <QtCore/QObject>

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavutil/frame.h>
}

namespace AVQt {
    enum class AudioCodec {
        MP3,
        AAC,
        VORBIS,
        OPUS,
        FLAC
    };
    struct AudioEncodeParameters {
        int32_t bitrate;
    };
    namespace api {
        class IAudioEncoderImpl {
        public:
            explicit IAudioEncoderImpl(const AudioEncodeParameters &parameters);
            virtual ~IAudioEncoderImpl() = default;

            [[nodiscard]] virtual const AudioEncodeParameters &getEncodeParameters() const;

            virtual bool open(const communication::AudioPadParams &params) = 0;
            virtual void close() = 0;

            virtual int encode(std::shared_ptr<AVFrame> frame) = 0;

            [[nodiscard]] virtual QVector<AVSampleFormat> getInputFormats() const = 0;
            [[nodiscard]] virtual std::shared_ptr<AVCodecParameters> getCodecParameters() const = 0;
            [[nodiscard]] virtual std::shared_ptr<communication::PacketPadParams> getPacketPadParams() const = 0;
        signals:
            virtual void packetReady(std::shared_ptr<AVPacket> packet) = 0;

        private:
            AudioEncodeParameters m_encodeParameters;
        };
    }// namespace api

    struct AudioEncoderInfo {
        QMetaObject metaObject;
        QString name;
        QList<common::Platform::Platform_t> platforms;
        QList<common::AudioFormat> supportedInputFormats;
        QList<AVCodecID> supportedCodecIds;
    };
}// namespace AVQt

Q_DECLARE_INTERFACE(AVQt::api::IAudioEncoderImpl, "avqt.api.IAudioEncoderImpl")

Q_DECLARE_METATYPE(AVQt::AudioEncodeParameters)

#endif//LIBAVQT_IAUDIOENCODERIMPL_HPP
