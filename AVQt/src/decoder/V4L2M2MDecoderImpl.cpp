//
// Created by silas on 18.02.22.
//

#include "V4L2M2MDecoderImpl.hpp"
#include "private/V4L2M2MDecoderImpl_p.hpp"

#include "AVQt/decoder/VideoDecoderFactory.hpp"

namespace AVQt {
    const api::VideoDecoderInfo &V4L2M2MDecoderImpl::info() {
        static const api::VideoDecoderInfo info{
                .metaObject = V4L2M2MDecoderImpl::staticMetaObject,
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
        return info;
    }

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
        d->init();
    }

    V4L2M2MDecoderImpl::~V4L2M2MDecoderImpl() noexcept {
        Q_D(V4L2M2MDecoderImpl);
        if (d->open) {
            V4L2M2MDecoderImpl::close();
        }
        d->hwDeviceContext.reset();
    }

    bool V4L2M2MDecoderImpl::open(std::shared_ptr<AVCodecParameters> codecParams) {
        Q_D(V4L2M2MDecoderImpl);

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            if (!d->codec) {
                qWarning() << "No codec found for codec id" << d->codecId;
                goto failed;
            }

            d->codecParameters = codecParams;

            d->codecContext.reset(avcodec_alloc_context3(d->codec));
            if (!d->codecContext) {
                qWarning() << "Could not allocate codec context";
                goto failed;
            }

            {
                auto hwFramesContext = av_hwframe_ctx_alloc(d->hwDeviceContext.get());
                auto framesContext = reinterpret_cast<AVHWFramesContext *>(hwFramesContext->data);
                framesContext->format = AV_PIX_FMT_DRM_PRIME;
                framesContext->sw_format = static_cast<AVPixelFormat>(d->codecParameters->format);
                framesContext->width = d->codecParameters->width;
                framesContext->height = d->codecParameters->height;

                int ret = av_hwframe_ctx_init(hwFramesContext);
                if (ret < 0) {
                    char errBuf[AV_ERROR_MAX_STRING_SIZE];
                    qWarning() << "Could not initialize frames context:" << av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                    goto failed;
                }
                d->hwFramesContext.reset(hwFramesContext);
            }

            if (avcodec_parameters_to_context(d->codecContext.get(), d->codecParameters.get()) < 0) {
                qWarning() << "Could not copy codec parameters to codec context";
                goto failed;
            }

            d->codecContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext.get());
            d->codecContext->get_format = &V4L2M2MDecoderImplPrivate::getFormat;
            d->codecContext->opaque = this;

            if (avcodec_open2(d->codecContext.get(), d->codec, nullptr) < 0) {
                qWarning() << "Could not open codec";
                goto failed;
            }

            d->frameFetcher = std::make_unique<V4L2M2MDecoderImplPrivate::FrameFetcher>(d);
            d->frameFetcher->start();
        }

        return true;

    failed:
        close();
        return false;
    }

    void V4L2M2MDecoderImpl::close() {
        Q_D(V4L2M2MDecoderImpl);

        if (d->frameFetcher) {
            d->frameFetcher->stop();
            d->frameFetcher.reset();
        }

        d->codecContext.reset();
        d->codecParameters.reset();
        d->hwFramesContext.reset();
    }

    int V4L2M2MDecoderImpl::decode(std::shared_ptr<AVPacket> packet) {
        Q_UNUSED(packet)
        Q_D(V4L2M2MDecoderImpl);

        if (!packet) {
            return EINVAL;
        }

        if (!d->codecContext) {
            qWarning() << "Codec context not initialized";
            return ENODEV;
        }

        {
            QMutexLocker lock(&d->codecMutex);

            int ret = avcodec_send_packet(d->codecContext.get(), packet.get());
            if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                char errBuf[AV_ERROR_MAX_STRING_SIZE];
                qWarning() << "Could not send packet to codec:" << av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                return AVUNERROR(ret);
            }

            d->firstFrame = false;
        }
        d->firstFrameCond.wakeAll();

        return EXIT_SUCCESS;
    }

    bool V4L2M2MDecoderImpl::isHWAccel() const {
        return true;
    }

    communication::VideoPadParams V4L2M2MDecoderImpl::getVideoParams() const {
        Q_D(const V4L2M2MDecoderImpl);

        communication::VideoPadParams params{};
        params.frameSize = {d->codecParameters->width, d->codecParameters->height};
        params.hwDeviceContext = d->hwDeviceContext;
        params.hwFramesContext = d->hwFramesContext;
        params.swPixelFormat = getSwOutputFormat();
        params.pixelFormat = getOutputFormat();
        params.isHWAccel = isHWAccel();

        return params;
    }

    AVPixelFormat V4L2M2MDecoderImpl::getOutputFormat() const {
        return AV_PIX_FMT_DRM_PRIME;
    }

    AVPixelFormat V4L2M2MDecoderImpl::getSwOutputFormat() const {
        Q_D(const V4L2M2MDecoderImpl);

        return static_cast<AVPixelFormat>(d->codecParameters->format);
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

        if (!codec) {
            qWarning() << "No codec found for codec" << avcodec_get_name(codecId);
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
            if (avcodec_is_open(ctx)) {
                avcodec_close(ctx);
            }
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

    AVPixelFormat V4L2M2MDecoderImplPrivate::getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts) {
        auto *decoder = reinterpret_cast<V4L2M2MDecoderImpl *>(ctx->opaque);

        auto *iFmt = pix_fmts;
        AVPixelFormat result = AV_PIX_FMT_NONE;
        while (*iFmt != AV_PIX_FMT_NONE) {
            if (*iFmt == AV_PIX_FMT_DRM_PRIME) {
                result = *iFmt;
                break;
            }
            ++iFmt;
        }

        if (result == AV_PIX_FMT_NONE) {
            qWarning() << "Could not find DRM_PRIME pixel format";
            return AV_PIX_FMT_NONE;
        }

        ctx->hw_frames_ctx = av_buffer_ref(decoder->d_func()->hwFramesContext.get());

        return result;
    }

    V4L2M2MDecoderImplPrivate::FrameFetcher::FrameFetcher(V4L2M2MDecoderImplPrivate *parent)
        : p(parent) {
        setObjectName("AVQt::V4L2M2MDecoderImpl::FrameFetcher");
    }

    void V4L2M2MDecoderImplPrivate::FrameFetcher::start() {
        bool shouldBe = false;
        if (running.compare_exchange_strong(shouldBe, true)) {
            QThread::start();
        } else {
            qWarning() << "FrameFetcher already running";
        }
    }

    void V4L2M2MDecoderImplPrivate::FrameFetcher::stop() {
        bool shouldBe = true;
        if (running.compare_exchange_strong(shouldBe, false)) {
            p->firstFrameCond.wakeAll();
            QThread::quit();
            QThread::wait();
        } else {
            qWarning() << "FrameFetcher not running";
        }
    }

    void V4L2M2MDecoderImplPrivate::FrameFetcher::run() {
        int ret;
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        while (running) {
            QMutexLocker lock(&p->codecMutex);
            if (p->firstFrame) {
                p->firstFrameCond.wait(&p->codecMutex);
                if (!running) {
                    break;
                }
            }
            std::shared_ptr<AVFrame> frame{av_frame_alloc(), [](AVFrame *f) {
                                               av_frame_free(&f);
                                           }};
            if (!frame) {
                qWarning() << "Could not allocate frame";
                continue;
            }
            ret = avcodec_receive_frame(p->codecContext.get(), frame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(ENOMEM)) {
                continue;
            } else if (ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                qWarning() << "Could not receive frame" << av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                continue;
            } else {
                p->q_func()->frameReady(frame);
            }
        }
    }
}// namespace AVQt

#ifdef Q_OS_LINUX
static_block {
    AVQt::VideoDecoderFactory::getInstance().registerDecoder(AVQt::V4L2M2MDecoderImpl::info());
}
#endif
