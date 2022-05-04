//
// Created by silas on 03/04/2022.
//

#include "QSVDecoderImpl.hpp"
#include "private/QSVDecoderImpl_p.hpp"

#include "common/PixelFormat.hpp"
#include "decoder/VideoDecoderFactory.hpp"

#include <QElapsedTimer>
#include <static_block.hpp>

namespace AVQt {
    const api::VideoDecoderInfo &QSVDecoderImpl::info() {
        static const api::VideoDecoderInfo info = {
                .metaObject = QSVDecoderImpl::staticMetaObject,
                .name = "QSV",
                .platforms = {common::Platform::Linux_X11, common::Platform::Linux_Wayland, common::Platform::Windows},
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
                        AV_CODEC_ID_H264,
                        AV_CODEC_ID_HEVC,
                        AV_CODEC_ID_AV1,
                        AV_CODEC_ID_VC1,
                        AV_CODEC_ID_VP8,
                        AV_CODEC_ID_VP9,
                        AV_CODEC_ID_MPEG2VIDEO,
                        AV_CODEC_ID_MJPEG,
                },
        };
        return info;
    }
    QSVDecoderImpl::QSVDecoderImpl(AVCodecID codecId, QObject *parent)
        : QObject(parent), d_ptr(new QSVDecoderImplPrivate(this)) {
        Q_D(QSVDecoderImpl);
        switch (codecId) {
            case AV_CODEC_ID_H264:
                d->pCodec = avcodec_find_decoder_by_name("h264_qsv");
                break;
            case AV_CODEC_ID_HEVC:
                d->pCodec = avcodec_find_decoder_by_name("hevc_qsv");
                break;
            case AV_CODEC_ID_AV1:
                d->pCodec = avcodec_find_decoder_by_name("av1_qsv");
                break;
            case AV_CODEC_ID_VC1:
                d->pCodec = avcodec_find_decoder_by_name("vc1_qsv");
                break;
            case AV_CODEC_ID_VP8:
                d->pCodec = avcodec_find_decoder_by_name("vp8_qsv");
                break;
            case AV_CODEC_ID_VP9:
                d->pCodec = avcodec_find_decoder_by_name("vp9_qsv");
                break;
            case AV_CODEC_ID_MPEG2VIDEO:
                d->pCodec = avcodec_find_decoder_by_name("mpeg2_qsv");
                break;
            case AV_CODEC_ID_MJPEG:
                d->pCodec = avcodec_find_decoder_by_name("mjpeg_qsv");
                break;
            default:
                throw std::runtime_error(std::string("Unsupported codec: ").append(avcodec_get_name(codecId)));
        }
    }

    QSVDecoderImpl::~QSVDecoderImpl() {
    }

    bool QSVDecoderImpl::open(std::shared_ptr<AVCodecParameters> codecParams) {
        Q_D(QSVDecoderImpl);

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            int ret;
            char strBuf[AV_ERROR_MAX_STRING_SIZE];

            d->codecParams = codecParams;

            d->codecContext.reset(avcodec_alloc_context3(d->pCodec));

            AVBufferRef *devCtx;
            ret = av_hwdevice_ctx_create(&devCtx, AV_HWDEVICE_TYPE_QSV, nullptr, nullptr, 0);
            if (ret < 0) {
                qWarning() << "[AVQt::QSVDecoderImpl] Failed to create HW device context: " << av_make_error_string(strBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                goto fail;
            }

            d->hwDeviceContext.reset(devCtx);

            auto *framesCtxRef = av_hwframe_ctx_alloc(d->hwDeviceContext.get());
            if (!framesCtxRef) {
                qWarning() << "[AVQt::QSVDecoderImpl] Failed to create HW frame context";
                goto fail;
            }

            auto *framesContext = reinterpret_cast<AVHWFramesContext *>(framesCtxRef->data);
            framesContext->format = AV_PIX_FMT_QSV;
            framesContext->sw_format = common::PixelFormat{static_cast<AVPixelFormat>(d->codecParams->format), AV_PIX_FMT_QSV}.toNativeFormat().getCPUFormat();
            framesContext->width = d->codecParams->width;
            framesContext->height = d->codecParams->height;
            framesContext->initial_pool_size = 32;

            ret = av_hwframe_ctx_init(framesCtxRef);
            if (ret < 0) {
                qWarning() << "[AVQt::QSVDecoderImpl] Failed to initialize HW frame context: " << av_make_error_string(strBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                goto fail;
            }

            d->hwFramesContext.reset(framesCtxRef);

            avcodec_parameters_to_context(d->codecContext.get(), d->codecParams.get());

            d->codecContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext.get());
            d->codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext.get());
            d->codecContext->opaque = d;
            d->codecContext->get_format = &QSVDecoderImplPrivate::getFormat;

            ret = avcodec_open2(d->codecContext.get(), d->pCodec, nullptr);
            if (ret < 0) {
                qWarning() << "[AVQt::QSVDecoderImpl] Failed to open codec: " << av_make_error_string(strBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                goto fail;
            }

            d->frameFetcher = std::make_unique<QSVDecoderImplPrivate::FrameFetcher>(d);
            d->frameFetcher->start();
            if (!d->frameFetcher->isRunning()) {
                qWarning() << "[AVQt::QSVDecoderImpl] Failed to start frame fetcher";
                goto fail;
            }

            return true;
        } else {
            qWarning("[AVQt::QSVDecoderImpl] Already opened");
            return false;
        }
    fail:
        d->open = false;
        d->frameFetcher.reset();
        d->codecContext.reset();
        d->hwDeviceContext.reset();
        d->hwFramesContext.reset();
        return false;
    }

    void QSVDecoderImpl::close() {
        Q_D(QSVDecoderImpl);

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            d->frameFetcher.reset();
            d->codecContext.reset();
            d->hwDeviceContext.reset();
            d->hwFramesContext.reset();
        }
    }

    int QSVDecoderImpl::decode(std::shared_ptr<AVPacket> packet) {
        Q_D(QSVDecoderImpl);

        if (!d->open) {
            qWarning("[AVQt::QSVDecoderImpl] Not opened");
            return ENODEV;
        }

        if (!packet) {
            qWarning("[AVQt::QSVDecoderImpl] Invalid packet");
            return EINVAL;
        }

        int ret = AVERROR(EXIT_SUCCESS);
        char strBuf[AV_ERROR_MAX_STRING_SIZE];

        {
            QMutexLocker locker(&d->codecMutex);
            ret = avcodec_send_packet(d->codecContext.get(), packet.get());
        }

        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            qWarning() << "[AVQt::QSVDecoderImpl] Failed to send packet: " << av_make_error_string(strBuf, AV_ERROR_MAX_STRING_SIZE, ret);
            return AVUNERROR(ret);
        }

        d->firstFrame = false;

        return AVUNERROR(ret);
    }

    AVPixelFormat QSVDecoderImpl::getOutputFormat() const {
        return AV_PIX_FMT_QSV;
    }

    AVPixelFormat QSVDecoderImpl::getSwOutputFormat() const {
        Q_D(const QSVDecoderImpl);
        if (d->codecParams) {
            return common::PixelFormat{static_cast<AVPixelFormat>(d->codecParams->format), AV_PIX_FMT_QSV}.toNativeFormat().getCPUFormat();
        } else {
            return AV_PIX_FMT_NONE;
        }
    }

    bool QSVDecoderImpl::isHWAccel() const {
        return true;
    }

    communication::VideoPadParams QSVDecoderImpl::getVideoParams() const {
        Q_D(const QSVDecoderImpl);
        if (d->codecParams && d->hwDeviceContext && d->hwFramesContext) {
            communication::VideoPadParams videoPadParams{};
            videoPadParams.frameSize = {d->codecParams->width, d->codecParams->height};
            videoPadParams.hwFramesContext = d->hwFramesContext;
            videoPadParams.hwDeviceContext = d->hwDeviceContext;
            videoPadParams.pixelFormat = getOutputFormat();
            videoPadParams.swPixelFormat = getSwOutputFormat();
            videoPadParams.isHWAccel = isHWAccel();
            return videoPadParams;
        }
        throw std::runtime_error("tried to get video params before initialization");
    }

    AVPixelFormat QSVDecoderImplPrivate::getFormat(AVCodecContext *codecContext, const AVPixelFormat *pixFmts) {
        auto *d = static_cast<QSVDecoderImplPrivate *>(codecContext->opaque);

        AVPixelFormat result = AV_PIX_FMT_NONE;
        for (int i = 0; pixFmts[i] != AV_PIX_FMT_NONE; i++) {
            if (pixFmts[i] == AV_PIX_FMT_QSV) {
                result = pixFmts[i];
                break;
            }
        }

        if (result == AV_PIX_FMT_NONE) {
            qWarning() << "No supported output format found";
            return AV_PIX_FMT_NONE;
        }

        codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext.get());

        return result;
    }

    void QSVDecoderImplPrivate::destroyAVCodecContext(AVCodecContext *codecContext) {
        if (codecContext) {
            if (avcodec_is_open(codecContext)) {
                avcodec_close(codecContext);
            }
            avcodec_free_context(&codecContext);
        }
    }

    void QSVDecoderImplPrivate::destroyAVBufferRef(AVBufferRef *buffer) {
        if (buffer) {
            av_buffer_unref(&buffer);
        }
    }

    QSVDecoderImplPrivate::FrameFetcher::FrameFetcher(QSVDecoderImplPrivate *d)
        : p(d) {
        setObjectName("AVQt::QSVDecoderImpl::FrameFetcher");
    }

    QSVDecoderImplPrivate::FrameFetcher::~FrameFetcher() {
        if (isRunning()) {
            stop();
        }
    }

    void QSVDecoderImplPrivate::FrameFetcher::stop() {
        bool shouldBe = false;
        if (m_stop.compare_exchange_strong(shouldBe, true)) {
            QThread::quit();
            QThread::wait();
        }
    }

    void QSVDecoderImplPrivate::FrameFetcher::run() {
        qDebug() << "FrameFetcher started";
        int ret;
        constexpr size_t strBufSize = 256;
        char strBuf[strBufSize];
        while (!m_stop) {
            QMutexLocker inputLock(&p->codecMutex);
            if (p->firstFrame) {
                inputLock.unlock();
                msleep(2);
                continue;
            }
            std::shared_ptr<AVFrame> frame = {av_frame_alloc(), [](AVFrame *frame) {
                                                  av_frame_free(&frame);
                                              }};
            ret = avcodec_receive_frame(p->codecContext.get(), frame.get());
            if (ret == 0) {
                qDebug("Publishing frame with PTS %ld", frame->pts);
                QElapsedTimer timer;
                timer.start();
                p->q_func()->frameReady(frame);
                inputLock.unlock();
                qDebug("Frame callback runtime: %lld ms", timer.elapsed());
            } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret == AVERROR(ENOMEM)) {
                frame.reset();
                inputLock.unlock();
                msleep(4);
            } else {
                inputLock.unlock();
                av_strerror(ret, strBuf, strBufSize);
                qWarning("Error while receiving frame: %s", strBuf);
                m_stop = true;
                break;
            }
        }
        qDebug() << "FrameFetcher stopped";
    }
}// namespace AVQt

static_block {
    AVQt::VideoDecoderFactory::getInstance().registerDecoder(AVQt::QSVDecoderImpl::info());
}
