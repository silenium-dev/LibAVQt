#ifndef LIBAVQT_MEDIACODECDECODERIMPL_HPP
#define LIBAVQT_MEDIACODECDECODERIMPL_HPP

#include "decoder/IVideoDecoderImpl.hpp"

namespace AVQt {
    class MediaCodecDecoderImplPrivate;
    class MediaCodecDecoderImpl : public QObject, public api::IVideoDecoderImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(MediaCodecDecoderImpl)
        Q_INTERFACES(AVQt::api::IVideoDecoderImpl)
    public:
        static const api::VideoDecoderInfo &info();

        Q_INVOKABLE explicit MediaCodecDecoderImpl(AVCodecID codecId, QObject *parent = nullptr);
        ~MediaCodecDecoderImpl() Q_DECL_OVERRIDE;

        bool open(std::shared_ptr<AVCodecParameters> codecParams) Q_DECL_OVERRIDE;
        void close() Q_DECL_OVERRIDE;
        int decode(std::shared_ptr<AVPacket> packet) Q_DECL_OVERRIDE;
        AVPixelFormat getOutputFormat() const Q_DECL_OVERRIDE;
        AVPixelFormat getSwOutputFormat() const Q_DECL_OVERRIDE;
        bool isHWAccel() const Q_DECL_OVERRIDE;
        communication::VideoPadParams getVideoParams() const Q_DECL_OVERRIDE;

    signals:
        void frameReady(std::shared_ptr<AVFrame> frame) Q_DECL_OVERRIDE;

    protected:
        Q_INVOKABLE MediaCodecDecoderImpl(AVCodecID codecId, AVQt::MediaCodecDecoderImplPrivate &dd, QObject *parent = nullptr);
        std::unique_ptr<MediaCodecDecoderImplPrivate> d_ptr;

    private:
        Q_DISABLE_COPY(MediaCodecDecoderImpl)
    };
}// namespace AVQt


#endif//LIBAVQT_MEDIACODECDECODERIMPL_HPP
