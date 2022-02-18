//
// Created by silas on 18.02.22.
//

#include "V4L2M2MDecoderImpl.hpp"
#include "private/V4L2M2MDecoderImpl_p.hpp"

namespace AVQt {
    const api::VideoDecoderInfo V4L2M2MDecoderImpl::info{
            .metaObject = &V4L2M2MDecoderImpl::staticMetaObject,
            .name = "V4L2",
            .platforms = {
                    common::Platform::Linux_Wayland,
                    common::Platform::Linux_X11,
                    common::Platform::Android,
            },
            .supportedInputPixelFormats = {
                    {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_YUV420P10, AV_PIX_FMT_NONE},
            },
            .supportedCodecIds = {
                    AV_CODEC_ID_MPEG1VIDEO,
                    AV_CODEC_ID_MPEG2VIDEO,
                    AV_CODEC_ID_MPEG4,
                    AV_CODEC_ID_H263,
                    AV_CODEC_ID_H264,
                    AV_CODEC_ID_HEVC,
                    AV_CODEC_ID_VC1,
                    AV_CODEC_ID_VP8,
                    AV_CODEC_ID_VP9,
            },
    };

    V4L2M2MDecoderImpl::V4L2M2MDecoderImpl(AVCodecID codec)
        : d_ptr(new V4L2M2MDecoderImplPrivate(this)) {
        Q_D(V4L2M2MDecoderImpl);
        d->codecId = codec;
        d->init();
    }

    V4L2M2MDecoderImpl::V4L2M2MDecoderImpl(AVCodecID codec, V4L2M2MDecoderImplPrivate &dd, QObject *parent)
        : QObject(parent), d_ptr(&dd) {
        Q_D(V4L2M2MDecoderImpl);
        d->codecId = codec;
    }

    V4L2M2MDecoderImpl::~V4L2M2MDecoderImpl() noexcept {
    }

    bool V4L2M2MDecoderImpl::open(std::shared_ptr<AVCodecParameters> codecParams) {
        Q_D(V4L2M2MDecoderImpl);

        return true;
    }

    void V4L2M2MDecoderImpl::close() {
    }

    int V4L2M2MDecoderImpl::decode(std::shared_ptr<AVPacket> packet) {
        Q_UNUSED(packet)

        return EXIT_SUCCESS;
    }

    bool V4L2M2MDecoderImpl::isHWAccel() const {
        return true;
    }

    communication::VideoPadParams V4L2M2MDecoderImpl::getVideoParams() const {
        throw std::runtime_error("Not implemented");
    }

    AVPixelFormat V4L2M2MDecoderImpl::getOutputFormat() const {
        throw std::runtime_error("Not implemented");
    }

    AVPixelFormat V4L2M2MDecoderImpl::getSwOutputFormat() const {
        throw std::runtime_error("Not implemented");
    }

    void V4L2M2MDecoderImplPrivate::init() {
        switch (codecId) {
            case AV_CODEC_ID_MPEG1VIDEO:
                codec = avcodec_find_decoder_by_name("mpeg1_v4l2m2m");
                break;
            case AV_CODEC_ID_MPEG2VIDEO:
                codec = avcodec_find_decoder_by_name("mpeg2_v4l2m2m");
                break;
            case AV_CODEC_ID_MPEG4:
                codec = avcodec_find_decoder_by_name("mpeg4_v4l2m2m");
                break;
            case AV_CODEC_ID_H263:
                codec = avcodec_find_decoder_by_name("h263_v4l2m2m");
                break;
            case AV_CODEC_ID_H264:
                codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
                break;
            case AV_CODEC_ID_HEVC:
                codec = avcodec_find_decoder_by_name("hevc_v4l2m2m");
                break;
            case AV_CODEC_ID_VC1:
                codec = avcodec_find_decoder_by_name("vc1_v4l2m2m");
                break;
            case AV_CODEC_ID_VP8:
                codec = avcodec_find_decoder_by_name("vp8_v4l2m2m");
                break;
            case AV_CODEC_ID_VP9:
                codec = avcodec_find_decoder_by_name("vp9_v4l2m2m");
                break;
            default:
                qWarning() << "Unsupported codec:" << avcodec_get_name(codecId);
                return;
        }

        int ret;
        char errBuf[AV_ERROR_MAX_STRING_SIZE];

        AVBufferRef *devCtx;
        ret = av_hwdevice_ctx_create(&devCtx, AV_HWDEVICE_TYPE_DRM, nullptr, nullptr, 0);
        if (ret < 0) {
            qWarning() << "Could not create DRM device context" << av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
            return;
        }
        hwDeviceContext.reset(devCtx);
    }

    void V4L2M2MDecoderImplPrivate::destroyAVCodecContext(AVCodecContext *ctx) {
        if (ctx) {
            avcodec_free_context(&ctx);
        }
    }

    void V4L2M2MDecoderImplPrivate::destroyAVCodecParameters(AVCodecParameters *par) {
        if (par) {
            avcodec_parameters_free(&par);
        }
    }
    void V4L2M2MDecoderImplPrivate::destroyAVBufferRef(AVBufferRef *buf) {
        if (buf) {
            av_buffer_unref(&buf);
        }
    }
}// namespace AVQt
