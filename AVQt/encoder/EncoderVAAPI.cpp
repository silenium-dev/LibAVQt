#include "EncoderVAAPI.h"
#include "private/EncoderVAAPI_p.h"
#include <QImage>

#include "output/IPacketSink.h"

namespace AVQt {
    EncoderVAAPI::EncoderVAAPI(CODEC codec, int bitrate, QObject *parent) : QThread(parent), d_ptr(new EncoderVAAPIPrivate(this)) {
        Q_D(AVQt::EncoderVAAPI);

        d->m_codec = codec;
        d->m_bitrate = bitrate;
    }

    [[maybe_unused]] EncoderVAAPI::EncoderVAAPI(EncoderVAAPIPrivate &p) : d_ptr(&p) {
    }

    EncoderVAAPI::EncoderVAAPI(EncoderVAAPI &&other) noexcept : d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    EncoderVAAPI::~EncoderVAAPI() {
        delete d_ptr;
    }

    int EncoderVAAPI::init() {
        Q_D(AVQt::EncoderVAAPI);

        //        int ret = 0;
        //        constexpr auto strBufSize = 64;
        //        char strBuf[strBufSize];

        std::string codec_name;
        switch (d->m_codec) {
            case CODEC::H264:
                codec_name = "h264_vaapi";
                break;
            case CODEC::HEVC:
                codec_name = "hevc_vaapi";
                break;
            case CODEC::VP9:
                if (qEnvironmentVariable("LIBVA_DRIVER_NAME") == "iHD") {
                    qFatal("[AVQt::EncoderVAAPI] Unsupported codec: VP9");
                } else {
                    codec_name = "vp9_vaapi";
                }
                break;
            case CODEC::VP8:
                codec_name = "vp8_vaapi";
                break;
            case CODEC::MPEG2:
                codec_name = "mpeg2_vaapi";
                break;
            case CODEC::AV1:
                qFatal("[AVQt::EncoderVAAPI] Unsupported codec: AV1");
        }

        d->m_pCodec = avcodec_find_encoder_by_name(codec_name.c_str());
        if (!d->m_pCodec) {
            qFatal("Could not find encoder: %s", codec_name.c_str());
        }

        return 0;
    }

    int EncoderVAAPI::deinit() {
        Q_D(AVQt::EncoderVAAPI);

        stop();

        {
            QMutexLocker lock(&d->m_cbListMutex);
            for (const auto &cb : d->m_cbList) {
                cb->deinit(this);
            }
        }

        if (d->m_pDeviceCtx) {
            av_buffer_unref(&d->m_pDeviceCtx);
        }

        d->m_pCodec = nullptr;

        if (d->m_pCodecCtx) {
            if (avcodec_is_open(d->m_pCodecCtx)) {
                avcodec_close(d->m_pCodecCtx);
            }
            avcodec_free_context(&d->m_pCodecCtx);
        }

        if (d->m_pFramesCtx) {
            av_buffer_unref(&d->m_pFramesCtx);
        }

        return 0;
    }

    int EncoderVAAPI::start() {
        Q_D(AVQt::EncoderVAAPI);

        bool shouldBe = false;
        if (d->m_running.compare_exchange_strong(shouldBe, true)) {
            d->m_paused.store(false);

            QThread::start();

            started();
        }

        return 0;
    }

    int EncoderVAAPI::stop() {
        Q_D(AVQt::EncoderVAAPI);

        bool shouldBe = true;
        if (d->m_running.compare_exchange_strong(shouldBe, false)) {
            d->m_paused.store(false);

            {
                QMutexLocker lock{&d->m_cbListMutex};
                for (const auto &cb : d->m_cbList) {
                    cb->stop(this);
                }
            }

            wait();

            {
                QMutexLocker lock{&d->m_inputQueueMutex};
                while (!d->m_inputQueue.isEmpty()) {
                    auto frame = d->m_inputQueue.dequeue();
                    av_frame_free(&frame.first);
                }
            }

            stopped();
        }

        return 0;
    }

    void EncoderVAAPI::pause(bool pause) {
        Q_D(AVQt::EncoderVAAPI);

        bool shouldBe = !pause;
        if (d->m_paused.compare_exchange_strong(shouldBe, pause)) {
            paused(pause);
        }
    }

    bool EncoderVAAPI::isPaused() {
        Q_D(AVQt::EncoderVAAPI);
        return d->m_paused.load();
    }

    int EncoderVAAPI::init(IFrameSource *source, AVRational framerate, int64_t duration) {
        Q_UNUSED(source)
        Q_UNUSED(framerate)
        Q_D(AVQt::EncoderVAAPI);
        d->m_duration = duration;
        init();
        return 0;
    }

    int EncoderVAAPI::deinit(IFrameSource *source) {
        Q_UNUSED(source)
        deinit();
        return 0;
    }

    int EncoderVAAPI::start(IFrameSource *source) {
        Q_UNUSED(source)
        start();
        return 0;
    }

    int EncoderVAAPI::stop(IFrameSource *source) {
        Q_UNUSED(source)
        stop();
        return 0;
    }

    void EncoderVAAPI::pause(IFrameSource *source, bool paused) {
        Q_UNUSED(source)
        pause(paused);
    }

    qint64 EncoderVAAPI::registerCallback(IPacketSink *packetSink, int8_t type) {
        Q_D(AVQt::EncoderVAAPI);

        if (type != IPacketSource::CB_VIDEO) {
            return -1;
        }

        {
            QMutexLocker lock{&d->m_cbListMutex};
            if (!d->m_cbList.contains(packetSink)) {
                d->m_cbList.append(packetSink);
                return d->m_cbList.indexOf(packetSink);
            } else {
                return -1;
            }
        }
    }

    qint64 EncoderVAAPI::unregisterCallback(IPacketSink *packetSink) {
        Q_D(AVQt::EncoderVAAPI);

        {
            QMutexLocker lock{&d->m_cbListMutex};
            auto count = d->m_cbList.removeAll(packetSink);
            return count > 0 ? count : -1;
        }
    }

    void EncoderVAAPI::onFrame(IFrameSource *source, AVFrame *frame, int64_t frameDuration, AVBufferRef *pDeviceCtx) {
        Q_UNUSED(source)
        Q_D(AVQt::EncoderVAAPI);

        QPair<AVFrame *, int64_t> queueFrame{av_frame_alloc(), frameDuration};
        switch (frame->format) {
                //            case AV_PIX_FMT_VAAPI:
            case AV_PIX_FMT_DRM_PRIME:
            case AV_PIX_FMT_DXVA2_VLD:
                qDebug("Transferring frame from GPU to CPU");
                av_hwframe_transfer_data(queueFrame.first, frame, 0);
                queueFrame.first->pts = frame->pts;
                break;
            default:
                qDebug("Referencing frame");
                av_frame_ref(queueFrame.first, frame);
                break;
        }

        while (d->m_inputQueue.size() > 4) {
            QThread::msleep(1);
        }
        {
            QMutexLocker lock{&d->m_inputQueueMutex};
            d->m_inputQueue.enqueue(queueFrame);
            //            std::sort(d->m_inputQueue.begin(), d->m_inputQueue.end(),
            //                      [&](const QPair<AVFrame *, int64_t> &f1, const QPair<AVFrame *, int64_t> &f2) {
            //                          return f1.first->pts < f2.first->pts;
            //                      });
        }
    }

    void EncoderVAAPI::run() {
        Q_D(AVQt::EncoderVAAPI);

        int ret;
        constexpr auto strBufSize{64};
        char strBuf[strBufSize];

        while (d->m_running.load()) {
            if (!d->m_paused.load() && d->m_inputQueue.size() > 1) {
                bool shouldBe = true;
                // Encoder open is only possible with frame parameters
                if (d->m_firstFrame.compare_exchange_strong(shouldBe, false)) {
                    auto frame = d->m_inputQueue.first().first;

                    d->m_pCodecCtx = avcodec_alloc_context3(d->m_pCodec);
                    if (!d->m_pCodecCtx) {
                        qFatal("Could not create VAAPI encoder context");
                    }
                    d->m_pCodecCtx->width = frame->width;
                    d->m_pCodecCtx->height = frame->height;
                    qDebug("Creating new HW context...");
                    ret = av_hwdevice_ctx_create(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
                    if (ret < 0) {
                        qFatal("%i: Unable to create AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    } else if (!d->m_pDeviceCtx) {
                        qFatal("Unable to create AVHWDeviceContext");
                    }

                    if (frame->hw_frames_ctx) {
                        d->m_pCodecCtx->sw_pix_fmt = static_cast<AVPixelFormat>(reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data)->sw_format);
                    } else {
                        d->m_pCodecCtx->sw_pix_fmt = static_cast<AVPixelFormat>(frame->format);
                    }

                    d->m_pFramesCtx = av_hwframe_ctx_alloc(d->m_pDeviceCtx);
                    auto *framesContext = reinterpret_cast<AVHWFramesContext *>(d->m_pFramesCtx->data);
                    framesContext->width = d->m_pCodecCtx->width;
                    framesContext->height = d->m_pCodecCtx->height;
                    framesContext->sw_format = d->m_pCodecCtx->sw_pix_fmt;
                    framesContext->format = AV_PIX_FMT_VAAPI;
                    framesContext->initial_pool_size = 64;

                    ret = av_hwframe_ctx_init(d->m_pFramesCtx);
                    if (ret != 0) {
                        qFatal("%i: Could not open HW frames context: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    d->m_pCodecCtx->pix_fmt = framesContext->format;

                    d->m_pCodecCtx->hw_device_ctx = av_buffer_ref(d->m_pDeviceCtx);
                    d->m_pCodecCtx->hw_frames_ctx = av_buffer_ref(d->m_pFramesCtx);
                    d->m_pCodecCtx->bit_rate = d->m_bitrate;
                    d->m_pCodecCtx->rc_min_rate = static_cast<int>(std::round(d->m_bitrate * 0.8));
                    d->m_pCodecCtx->rc_max_rate = static_cast<int>(std::round(d->m_bitrate * 1.1));
                    d->m_pCodecCtx->rc_buffer_size = d->m_bitrate * 2;
                    d->m_pCodecCtx->gop_size = 20;
                    d->m_pCodecCtx->max_b_frames = 0;
                    d->m_pCodecCtx->color_primaries = AVCOL_PRI_BT2020;
                    d->m_pCodecCtx->color_trc = AVCOL_TRC_SMPTE2084;
                    d->m_pCodecCtx->colorspace = AVCOL_SPC_BT2020_NCL;

                    // Timestamps from frame sources are always microseconds, trying to use this timebase for the encoder too
                    d->m_pCodecCtx->time_base = av_make_q(1, 1000000);
                    ret = avcodec_open2(d->m_pCodecCtx, d->m_pCodec, nullptr);
                    if (ret < 0) {
                        qFatal("%i: Unable to open VAAPI encoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                    qDebug("[AVQt::EncoderVAAPI] Encoder timebase: %d/%d", d->m_pCodecCtx->time_base.num, d->m_pCodecCtx->time_base.den);

                    // Preallocating frame on gpu
                    d->m_pHWFrame = av_frame_alloc();
                    ret = av_hwframe_get_buffer(d->m_pFramesCtx, d->m_pHWFrame, 0);
                    if (ret != 0) {
                        qFatal("%i: Could not allocate HW frame in encoder context: %s", ret,
                               av_make_error_string(strBuf, strBufSize, ret));
                    }

                    QMutexLocker lock(&d->m_cbListMutex);
                    for (const auto &cb : d->m_cbList) {
                        AVCodecParameters *parameters = avcodec_parameters_alloc();
                        avcodec_parameters_from_context(parameters, d->m_pCodecCtx);
                        parameters->bit_rate = d->m_bitrate;
                        cb->init(this, av_make_q(0, 1), d->m_pCodecCtx->time_base, d->m_duration, parameters, nullptr, nullptr);
                        cb->start(this);
                    }
                }
                if (!d->m_inputQueue.isEmpty()) {
                    QPair<AVFrame *, int64_t> frame;
                    {
                        QMutexLocker lock(&d->m_inputQueueMutex);
                        frame = d->m_inputQueue.dequeue();
                    }
                    ret = av_hwframe_map(d->m_pHWFrame, frame.first, AV_HWFRAME_MAP_READ);

                    qWarning("Transfer: %d: %s", ret, av_make_error_string(strBuf, strBufSize, ret));

                    d->m_pHWFrame->pts = av_rescale_q(frame.first->pts, av_make_q(1, 1000000),
                                                      d->m_pCodecCtx->time_base);// Incoming timestamps are always microseconds
                                                                                 //                    d->m_pHWFrame->pts = frame.first->pts;
                                                                                 //                    av_frame_free(&frame.first);
                    AVFrame *swFrame = av_frame_alloc();
                    av_hwframe_transfer_data(swFrame, d->m_pHWFrame, 0);
                    QImage image{swFrame->data[0], d->m_pHWFrame->width, d->m_pHWFrame->height, swFrame->linesize[0], QImage::Format_Grayscale8};
                    image.save("output.bmp");
                    qApp->quit();
                    ret = avcodec_send_frame(d->m_pCodecCtx, d->m_pHWFrame);
                    qDebug("[AVQt::EncoderVAAPI] Sent frame with PTS %lld to encoder", static_cast<long long>(d->m_pHWFrame->pts));
                    /*if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else */
                    if (ret != 0) {
                        qFatal("%i: Could not send frame to VAAPI encoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    //                    avcodec_send_frame(d->m_pCodecCtx, nullptr);
                    //
                    //                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    //                        break;
                    //                    } else if (ret != 0) {
                    //                        qFatal("%i: Could not flush VAAPI encoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    //                    }
                }
                AVPacket *packet = av_packet_alloc();
                while (true) {
                    ret = avcodec_receive_packet(d->m_pCodecCtx, packet);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret != 0) {
                        qFatal("%i: Could not receive packet from encoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                    //                    packet->pts = d->m_pHWFrame->pts;
                    //                    packet->pts = d->m_pCodecCtx->coded_frame->pts;
                    //                    packet->pts = packet->dts;
                    packet->pos = -1;
                    av_packet_rescale_ts(packet, d->m_pCodecCtx->time_base, av_make_q(1, 1000000));
                    qDebug("[AVQt::EncoderVAAPI] Got packet from encoder with PTS: %lld, DTS: %lld, duration: %lld, timebase: %d/%d",
                           static_cast<long long>(packet->pts),
                           static_cast<long long>(packet->dts),
                           static_cast<long long>(packet->duration),
                           d->m_pCodecCtx->time_base.num, d->m_pCodecCtx->time_base.den);
                    if (packet->buf) {
                        QMutexLocker lock(&d->m_cbListMutex);
                        for (const auto &cb : d->m_cbList) {
                            AVPacket *cbPacket = av_packet_clone(packet);
                            cb->onPacket(this, cbPacket, IPacketSource::CB_VIDEO);
                            av_packet_free(&cbPacket);
                        }
                    }
                    av_packet_unref(packet);
                }
            } else {
                msleep(1);
            }
        }
    }

    EncoderVAAPI &EncoderVAAPI::operator=(EncoderVAAPI &&other) noexcept {
        delete d_ptr;
        d_ptr = other.d_ptr;
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;

        return *this;
    }
}// namespace AVQt
