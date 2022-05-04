#ifndef LIBAVQT_IAUDIODECODERIMPL_HPP
#define LIBAVQT_IAUDIODECODERIMPL_HPP

#include "AVQt/common/AudioFormat.hpp"
#include "AVQt/common/Platform.hpp"
#include "AVQt/communication/AudioPadParams.hpp"

#include <memory>

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
}

namespace AVQt::api {
    class IAudioDecoderImpl {
    public:
        virtual ~IAudioDecoderImpl() = default;

        virtual bool open(std::shared_ptr<AVCodecParameters> codecParams) = 0;
        virtual void close() = 0;

        virtual int decode(std::shared_ptr<AVPacket> packet) = 0;

        [[nodiscard]] virtual common::AudioFormat getOutputFormat() const = 0;
        [[nodiscard]] virtual communication::AudioPadParams getAudioParams() const = 0;

    signals:
        virtual void frameReady(std::shared_ptr<AVFrame> frame) = 0;
    };

    struct AudioDecoderInfo {
        QMetaObject metaObject;
        QString name;
        QList<common::Platform::Platform_t> platforms;

        /**
         * @brief Contains the list of supported formats.
         * If empty, all formats are supported.
         */
        QList<common::AudioFormat> supportedInputAudioFormats;

        /**
         * @brief Contains the list of supported codecs.
         * If empty, all codecs ffmpeg is compiled with are supported.
         */
        QList<AVCodecID> supportedCodecIds;
    };
}// namespace AVQt::api

Q_DECLARE_INTERFACE(AVQt::api::IAudioDecoderImpl, "AVQt.api.IAudioDecoderImpl")

#endif//LIBAVQT_IAUDIODECODERIMPL_HPP
