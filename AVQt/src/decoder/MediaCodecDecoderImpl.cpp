#include "MediaCodecDecoderImpl.hpp"
#include "private/MediaCodecDecoderImpl_p.hpp"

#include "AVQt/decoder/VideoDecoderFactory.hpp"

#include <static_block.hpp>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace AVQt {
    const api::VideoDecoderInfo &MediaCodecDecoderImpl::info() {
        static const api::VideoDecoderInfo info = {
                .metaObject = MediaCodecDecoderImpl::staticMetaObject,
                .name = "MediaCodec",
                .platforms = {
                        common::Platform::Android,
                },
                .supportedInputPixelFormats = {
                        {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE},
                        {AV_PIX_FMT_YUV420P10, AV_PIX_FMT_NONE},
                        {AV_PIX_FMT_YUV420P12, AV_PIX_FMT_NONE},
                        {AV_PIX_FMT_YUV420P14, AV_PIX_FMT_NONE},
                        {AV_PIX_FMT_YUV420P16, AV_PIX_FMT_NONE},
                        {AV_PIX_FMT_NV12, AV_PIX_FMT_NONE},
                        {AV_PIX_FMT_P010, AV_PIX_FMT_NONE},
                        {AV_PIX_FMT_P016, AV_PIX_FMT_NONE},
                },
                .supportedCodecIds = {
                        // TODO: Dynamically fetch supported codec IDs
                        AV_CODEC_ID_H263,
                        AV_CODEC_ID_H264,
                        AV_CODEC_ID_HEVC,
                        AV_CODEC_ID_MPEG4,
                        AV_CODEC_ID_VP8,
                        AV_CODEC_ID_VP9,
                        AV_CODEC_ID_AV1,
                },
        };
        return info;
    }

    MediaCodecDecoderImpl::MediaCodecDecoderImpl(AVCodecID codecId, QObject *parent)
        : QObject(parent), d_ptr(new MediaCodecDecoderImplPrivate(this)) {
        Q_D(MediaCodecDecoderImpl);
        d->codecId = codecId;
        d->init();
    }

    MediaCodecDecoderImpl::MediaCodecDecoderImpl(AVCodecID codecId, MediaCodecDecoderImplPrivate &dd, QObject *parent)
        : QObject(parent), d_ptr(&dd) {
        Q_D(MediaCodecDecoderImpl);
        d->codecId = codecId;
        d->init();
    }

    MediaCodecDecoderImpl::~MediaCodecDecoderImpl() {
        Q_D(MediaCodecDecoderImpl);

        if (d->open) {
            MediaCodecDecoderImpl::close();
        }

        d->hwDeviceContext.reset();
    }

    bool MediaCodecDecoderImpl::open(std::shared_ptr<AVCodecParameters> codecParams) {
        Q_D(MediaCodecDecoderImpl);

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
                if (!hwFramesContext) {
                    qWarning() << "Could not allocate HW frames context";
                    goto failed;
                }
                auto framesContext = reinterpret_cast<AVHWFramesContext *>(hwFramesContext->data);

                framesContext->format = AV_PIX_FMT_MEDIACODEC;
                framesContext->sw_format = static_cast<AVPixelFormat>(d->codecParameters->format);
                framesContext->width = d->codecParameters->width;
                framesContext->height = d->codecParameters->height;
                framesContext->initial_pool_size = 0;

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
            d->codecContext->get_format = &MediaCodecDecoderImplPrivate::getFormat;
            d->codecContext->opaque = this;
            d->codecContext->pix_fmt = AV_PIX_FMT_MEDIACODEC;

            int ret = avcodec_open2(d->codecContext.get(), d->codec, nullptr);
            if (ret < 0) {
                char errBuf[AV_ERROR_MAX_STRING_SIZE];
                qWarning() << "Could not open codec:" << av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                goto failed;
            }

            d->frameFetcher = std::make_unique<MediaCodecDecoderImplPrivate::FrameFetcher>(d);
            d->frameFetcher->start();

            return true;
        }

        return false;

    failed:
        close();
        return false;
    }

    void MediaCodecDecoderImpl::close() {
        Q_D(MediaCodecDecoderImpl);

        if (d->frameFetcher) {
            d->frameFetcher->stop();
            d->frameFetcher.reset();
        }

        d->codecContext.reset();
        d->codecParameters.reset();
        d->hwFramesContext.reset();
    }

    int MediaCodecDecoderImpl::decode(std::shared_ptr<AVPacket> packet) {
        Q_UNUSED(packet)
        Q_D(MediaCodecDecoderImpl);

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
            qDebug() << "Packet size:" << (packet ? packet->size : -1);
            if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                char errBuf[AV_ERROR_MAX_STRING_SIZE];
                qWarning() << "Could not send packet to codec:" << av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                return AVUNERROR(ret);
            } else if (ret == 0) {
                qDebug() << "Sent packet";
            }

            d->firstFrame = false;
        }
        d->firstFrameCond.wakeAll();

        return EXIT_SUCCESS;
    }

    AVPixelFormat MediaCodecDecoderImpl::getOutputFormat() const {
        return AV_PIX_FMT_MEDIACODEC;
    }

    AVPixelFormat MediaCodecDecoderImpl::getSwOutputFormat() const {
        Q_D(const MediaCodecDecoderImpl);

        return static_cast<AVPixelFormat>(d->codecParameters->format);
    }

    bool MediaCodecDecoderImpl::isHWAccel() const {
        return true;
    }

    communication::VideoPadParams MediaCodecDecoderImpl::getVideoParams() const {
        Q_D(const MediaCodecDecoderImpl);

        communication::VideoPadParams params{};
        params.frameSize = {d->codecParameters->width, d->codecParameters->height};
        params.hwDeviceContext = d->hwDeviceContext;
        params.hwFramesContext = d->hwFramesContext;
        params.swPixelFormat = getSwOutputFormat();
        params.pixelFormat = getOutputFormat();
        params.isHWAccel = isHWAccel();

        return params;
    }

    void MediaCodecDecoderImplPrivate::init() {
        Q_Q(MediaCodecDecoderImpl);

        if (!MediaCodecDecoderImpl::info().supportedCodecIds.contains(codecId)) {
            qWarning() << "Unsupported codec:" << avcodec_get_name(codecId);
            return;
        }

        switch (codecId) {
            case AV_CODEC_ID_MPEG2VIDEO:
                codec = avcodec_find_decoder_by_name("mpeg2_mediacodec");
                break;
            case AV_CODEC_ID_MPEG4:
                codec = avcodec_find_decoder_by_name("mpeg4_mediacodec");
                break;
            case AV_CODEC_ID_H264:
                codec = avcodec_find_decoder_by_name("h264_mediacodec");
                break;
            case AV_CODEC_ID_HEVC:
                codec = avcodec_find_decoder_by_name("hevc_mediacodec");
                break;
            case AV_CODEC_ID_VP8:
                codec = avcodec_find_decoder_by_name("vp8_mediacodec");
                break;
            case AV_CODEC_ID_VP9:
                codec = avcodec_find_decoder_by_name("vp9_mediacodec");
                break;
            case AV_CODEC_ID_AV1:
                codec = avcodec_find_decoder_by_name("av1_mediacodec");
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

        AVBufferRef *devCtx{nullptr};
        ret = av_hwdevice_ctx_create(&devCtx, AV_HWDEVICE_TYPE_MEDIACODEC, nullptr, nullptr, 0);
        if (ret < 0) {
            qWarning() << "Could not create MediaCodec device context" << av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
            return;
        }
        hwDeviceContext.reset(devCtx);
    }

    void MediaCodecDecoderImplPrivate::destroyAVCodecContext(AVCodecContext *ctx) {
        if (ctx) {
            if (avcodec_is_open(ctx)) {
                avcodec_close(ctx);
            }
            avcodec_free_context(&ctx);
        }
    }

    void MediaCodecDecoderImplPrivate::destroyAVCodecParameters(AVCodecParameters *par) {
        if (par) {
            avcodec_parameters_free(&par);
        }
    }

    void MediaCodecDecoderImplPrivate::destroyAVBufferRef(AVBufferRef *buf) {
        if (buf) {
            av_buffer_unref(&buf);
        }
    }

    AVPixelFormat MediaCodecDecoderImplPrivate::getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts) {
        auto *decoder = reinterpret_cast<MediaCodecDecoderImpl *>(ctx->opaque);

        auto *iFmt = pix_fmts;
        AVPixelFormat result = AV_PIX_FMT_NONE;
        while (*iFmt != AV_PIX_FMT_NONE) {
            if (*iFmt == AV_PIX_FMT_MEDIACODEC) {
                result = *iFmt;
                break;
            }
            ++iFmt;
        }

        if (result == AV_PIX_FMT_NONE) {
            qWarning() << "Could not find MediaCodec pixel format";
            return AV_PIX_FMT_NONE;
        }

        ctx->hw_frames_ctx = av_buffer_ref(decoder->d_func()->hwFramesContext.get());

        return result;
    }

    MediaCodecDecoderImplPrivate::FrameFetcher::FrameFetcher(MediaCodecDecoderImplPrivate *parent)
        : p(parent) {
        setObjectName("AVQt::mediacodecDecoderImpl::FrameFetcher");
    }

    void MediaCodecDecoderImplPrivate::FrameFetcher::start() {
        bool shouldBe = false;
        if (running.compare_exchange_strong(shouldBe, true)) {
            QThread::start();
        } else {
            qWarning() << "FrameFetcher already running";
        }
    }

    void MediaCodecDecoderImplPrivate::FrameFetcher::stop() {
        bool shouldBe = true;
        if (running.compare_exchange_strong(shouldBe, false)) {
            p->firstFrameCond.wakeAll();
            QThread::quit();
            QThread::wait();
        } else {
            qWarning() << "FrameFetcher not running";
        }
    }

    void MediaCodecDecoderImplPrivate::FrameFetcher::run() {
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
            frame->format = AV_PIX_FMT_MEDIACODEC;
            ret = avcodec_receive_frame(nullptr, frame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(ENOMEM)) {
                usleep(500);
                continue;
            } else if (ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                qWarning() << "Could not receive frame" << av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                continue;
            } else {
                emit p->q_func()->frameReady(frame);
            }
        }
    }
}// namespace AVQt

#ifdef Q_OS_ANDROID
static_block {
    AVQt::VideoDecoderFactory::getInstance().registerDecoder(AVQt::MediaCodecDecoderImpl::info());
}
#endif
