//
// Created by silas on 4/18/21.
//

#include "private/EncoderQSV_p.h"
#include "EncoderQSV.h"

#include "output/IPacketSink.h"

namespace AVQt {
    EncoderQSV::EncoderQSV(CODEC codec, int bitrate, QObject *parent) : QThread(parent), d_ptr(new EncoderQSVPrivate(this)) {
        Q_D(AVQt::EncoderQSV);

        d->m_codec = codec;
        d->m_bitrate = bitrate;
    }

    [[maybe_unused]] EncoderQSV::EncoderQSV(EncoderQSVPrivate &p) : d_ptr(&p) {

    }

    EncoderQSV::EncoderQSV(EncoderQSV &&other) noexcept: d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    EncoderQSV::~EncoderQSV() {
        delete d_ptr;
    }

    int EncoderQSV::init() {
        Q_D(AVQt::EncoderQSV);

//        int ret = 0;
//        constexpr auto strBufSize = 64;
//        char strBuf[strBufSize];

        std::string codec_name;
        switch (d->m_codec) {
            case CODEC::H264:
                codec_name = "h264_qsv";
                break;
            case CODEC::HEVC:
                codec_name = "hevc_qsv";
                break;
            case CODEC::VP9:
                if (qEnvironmentVariable("LIBVA_DRIVER_NAME") == "iHD") {
                    qFatal("[AVQt::EncoderQSV] Unsupported codec: VP9");
                } else {
                    codec_name = "vp9_qsv";
                }
                break;
            case CODEC::VP8:
                codec_name = "vp8_qsv";
                break;
            case CODEC::MPEG2:
                codec_name = "mpeg2_qsv";
                break;
            case CODEC::AV1:
                qFatal("[AVQt::EncoderQSV] Unsupported codec: AV1");
        }

        d->m_pCodec = avcodec_find_encoder_by_name(codec_name.c_str());
        if (!d->m_pCodec) {
            qFatal("Could not find encoder: %s", codec_name.c_str());
        }

        return 0;
    }

    int EncoderQSV::deinit() {
        Q_D(AVQt::EncoderQSV);

        stop();

        {
            QMutexLocker lock(&d->m_cbListMutex);
            for (const auto &cb: d->m_cbList) {
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

        d->m_framerate = {0, 1};

        d->m_pLockedSource = nullptr;

        return 0;
    }

    int EncoderQSV::start() {
        Q_D(AVQt::EncoderQSV);

        bool shouldBe = false;
        if (d->m_running.compare_exchange_strong(shouldBe, true)) {
            d->m_paused.store(false);

            QThread::start();

            started();
        }

        return 0;
    }

    int EncoderQSV::stop() {
        Q_D(AVQt::EncoderQSV);

        bool shouldBe = true;
        if (d->m_running.compare_exchange_strong(shouldBe, false)) {
            d->m_paused.store(false);

            {
                QMutexLocker lock{&d->m_cbListMutex};
                for (const auto &cb: d->m_cbList) {
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

    void EncoderQSV::pause(bool pause) {
        Q_D(AVQt::EncoderQSV);

        bool shouldBe = !pause;
        if (d->m_paused.compare_exchange_strong(shouldBe, pause)) {
            paused(pause);
        }
    }

    bool EncoderQSV::isPaused() {
        Q_D(AVQt::EncoderQSV);
        return d->m_paused.load();
    }

    int EncoderQSV::init(IFrameSource *source, AVRational framerate, int64_t duration) {
        Q_D(AVQt::EncoderQSV);

        if (d->m_pLockedSource) {
            qFatal("[AVQt::EncoderQSV] Already initialized and bound to frame source");
        }

        d->m_pLockedSource = source;
        d->m_framerate = framerate;
        d->m_duration = duration;
        init();
        return 0;
    }

    int EncoderQSV::deinit(IFrameSource *source) {
        Q_D(AVQt::EncoderQSV);
        if (d->m_pLockedSource != source) {
            qFatal("[AVQt::EncoderQSV] Cleaning up encoder from other source than initialized is forbidden");
        }

        deinit();
        return 0;
    }

    int EncoderQSV::start(IFrameSource *source) {
        Q_D(AVQt::EncoderQSV);
        if (d->m_pLockedSource != source) {
            qFatal("[AVQt::EncoderQSV] Starting encoder from other source than initialized is forbidden");
        }

        start();
        return 0;
    }

    int EncoderQSV::stop(IFrameSource *source) {
        Q_D(AVQt::EncoderQSV);
        if (d->m_pLockedSource != source) {
            qFatal("[AVQt::EncoderQSV] Stopping encoder from other source than initialized is forbidden");
        }

        stop();
        return 0;
    }

    void EncoderQSV::pause(IFrameSource *source, bool paused) {
        Q_D(AVQt::EncoderQSV);
        if (d->m_pLockedSource != source) {
            qFatal("[AVQt::EncoderQSV] Pausing encoder from other source than initialized is forbidden");
        }

        pause(paused);
    }

    qint64 EncoderQSV::registerCallback(IPacketSink *packetSink, int8_t type) {
        Q_D(AVQt::EncoderQSV);

        if (type != IPacketSource::CB_VIDEO) {
            return -1;
        }

        {
            QMutexLocker lock{&d->m_cbListMutex};
            if (!d->m_cbList.contains(packetSink)) {
                d->m_cbList.append(packetSink);
                if (d->m_running.load() && d->m_pCodecCtx) {
                    AVCodecParameters *parameters = avcodec_parameters_alloc();
                    avcodec_parameters_from_context(parameters, d->m_pCodecCtx);
                    packetSink->init(this, d->m_framerate, d->m_pCodecCtx->time_base, d->m_duration, parameters, nullptr, nullptr);
                    packetSink->start(this);
                }
                return d->m_cbList.indexOf(packetSink);
            } else {
                return -1;
            }
        }
    }

    qint64 EncoderQSV::unregisterCallback(IPacketSink *packetSink) {
        Q_D(AVQt::EncoderQSV);

        {
            QMutexLocker lock{&d->m_cbListMutex};
            auto count = d->m_cbList.removeAll(packetSink);
            packetSink->deinit(this);
            return count > 0 ? count : -1;
        }
    }

    void EncoderQSV::onFrame(IFrameSource *source, AVFrame *frame, int64_t frameDuration, AVBufferRef *pDeviceCtx) {
        Q_D(AVQt::EncoderQSV);

        if (d->m_pLockedSource != source) {
            qFatal("[AVQt::EncoderQSV] Sending frames from other source than initialized is forbidden");
        }

        int ret;
        constexpr auto strBufSize = 64;
        char strBuf[strBufSize];

        QPair<AVFrame *, int64_t> queueFrame{av_frame_alloc(), frameDuration};
        switch (frame->format) {
            case AV_PIX_FMT_QSV:
                if (!d->m_pDeviceCtx) {
                    d->m_pDeviceCtx = av_buffer_ref(pDeviceCtx);
                    d->m_pFramesCtx = av_buffer_ref(frame->hw_frames_ctx);
                }
                qDebug("Referencing frame");
                av_frame_ref(queueFrame.first, frame);
                break;
            case AV_PIX_FMT_DRM_PRIME:
            case AV_PIX_FMT_VAAPI:
                if (!d->m_pDeviceCtx) {
                    ret = av_hwdevice_ctx_create_derived(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_QSV, pDeviceCtx, 0);
                    if (ret != 0) {
                        qFatal("[AVQt::EncoderQSV] %d: Could not create derived AVHWDeviceContext: %s", ret,
                               av_make_error_string(strBuf, strBufSize, ret));
                    }
                    ret = av_hwframe_ctx_create_derived(&d->m_pFramesCtx, AV_PIX_FMT_QSV, d->m_pDeviceCtx, frame->hw_frames_ctx,
                                                        AV_HWFRAME_MAP_READ);
                    if (ret != 0) {
                        qFatal("[AVQt::EncoderQSV] %d: Could not create derived AVHWFramesContext: %s", ret,
                               av_make_error_string(strBuf, strBufSize, ret));
                    }

                    ret = av_hwframe_ctx_init(d->m_pFramesCtx);
                    if (ret != 0) {
                        qFatal("[AVQt::EncoderQSV] %d: Could not init HW frames context: %s", ret,
                               av_make_error_string(strBuf, strBufSize, ret));
                    }
                }
                av_frame_ref(queueFrame.first, frame);
                break;
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

    void EncoderQSV::run() {
        Q_D(AVQt::EncoderQSV);

        int ret;
        constexpr auto strBufSize{64};
        char strBuf[strBufSize];

        while (d->m_running.load()) {
            if (!d->m_paused.load() && d->m_inputQueue.size() > 1) {
                bool shouldBe = true;
                // Encoder init is only possible with frame parameters
                if (d->m_firstFrame.compare_exchange_strong(shouldBe, false)) {
                    auto frame = d->m_inputQueue.first().first;

                    d->m_pCodecCtx = avcodec_alloc_context3(d->m_pCodec);
                    if (!d->m_pCodecCtx) {
                        qFatal("[AVQt::EncoderQSV] Could not create QSV encoder context");
                    }
                    d->m_pCodecCtx->width = frame->width;
                    d->m_pCodecCtx->height = frame->height;
                    if (!d->m_pDeviceCtx) {
                        qDebug("[AVQt::EncoderQSV] Creating new HW context...");
                        ret = av_hwdevice_ctx_create(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_QSV, nullptr, nullptr, 0);
                        if (ret < 0) {
                            qFatal("[AVQt::EncoderQSV] %i: Unable to create AVHWDeviceContext: %s", ret,
                                   av_make_error_string(strBuf, strBufSize, ret));
                        } else if (!d->m_pDeviceCtx) {
                            qFatal("[AVQt::EncoderQSV] Unable to create AVHWDeviceContext");
                        }

                        d->m_pCodecCtx->sw_pix_fmt = static_cast<AVPixelFormat>(frame->format);

                        d->m_pFramesCtx = av_hwframe_ctx_alloc(d->m_pDeviceCtx);
                        auto *framesContext = reinterpret_cast<AVHWFramesContext *>(d->m_pFramesCtx->data);
                        framesContext->width = d->m_pCodecCtx->width;
                        framesContext->height = d->m_pCodecCtx->height;
                        framesContext->sw_format = d->m_pCodecCtx->sw_pix_fmt;
                        framesContext->format = AV_PIX_FMT_QSV;
                        framesContext->initial_pool_size = 20;

                        ret = av_hwframe_ctx_init(d->m_pFramesCtx);
                        if (ret != 0) {
                            qFatal("[AVQt::EncoderQSV] %i: Could not init HW frames context: %s", ret,
                                   av_make_error_string(strBuf, strBufSize, ret));
                        }
                        d->m_pCodecCtx->pix_fmt = framesContext->format;

                        // Preallocating frame on gpu
                        d->m_pHWFrame = av_frame_alloc();
                        ret = av_hwframe_get_buffer(d->m_pFramesCtx, d->m_pHWFrame, 0);
                        if (ret != 0) {
                            qFatal("[AVQt::EncoderQSV] %i: Could not allocate HW frame in encoder context: %s", ret,
                                   av_make_error_string(strBuf, strBufSize, ret));
                        }
                    }
                    d->m_pCodecCtx->hw_device_ctx = av_buffer_ref(d->m_pDeviceCtx);
                    d->m_pCodecCtx->hw_frames_ctx = av_buffer_ref(d->m_pFramesCtx);
                    d->m_pCodecCtx->pix_fmt = AV_PIX_FMT_QSV;
                    d->m_pCodecCtx->bit_rate = d->m_bitrate;
                    d->m_pCodecCtx->rc_min_rate = static_cast<int>(std::round(d->m_bitrate * 0.8));
                    d->m_pCodecCtx->rc_max_rate = static_cast<int>(std::round(d->m_bitrate * 1.1));
                    d->m_pCodecCtx->rc_buffer_size = d->m_bitrate * 2;
                    d->m_pCodecCtx->gop_size = 20;
                    d->m_pCodecCtx->max_b_frames = 0;
                    d->m_pCodecCtx->color_primaries = AVCOL_PRI_BT2020;
                    d->m_pCodecCtx->color_trc = AVCOL_TRC_SMPTE2084;
                    d->m_pCodecCtx->colorspace = AVCOL_SPC_BT2020_NCL;
                    d->m_pCodecCtx->framerate = d->m_framerate;

                    // Timestamps from frame sources are always microseconds, trying to use this timebase for the encoder too
                    d->m_pCodecCtx->time_base = av_make_q(1, 1000000);
                    ret = avcodec_open2(d->m_pCodecCtx, d->m_pCodec, nullptr);
                    if (ret < 0) {
                        qFatal("[AVQt::EncoderQSV] %i: Unable to open QSV encoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                    qDebug("[AVQt::EncoderQSV] Encoder timebase: %d/%d", d->m_pCodecCtx->time_base.num, d->m_pCodecCtx->time_base.den);

                    QMutexLocker lock(&d->m_cbListMutex);
                    for (const auto &cb: d->m_cbList) {
                        AVCodecParameters *parameters = avcodec_parameters_alloc();
                        avcodec_parameters_from_context(parameters, d->m_pCodecCtx);
                        cb->init(this, d->m_framerate, d->m_pCodecCtx->time_base, 0, parameters, nullptr, nullptr);
                        cb->start(this);
                    }
                }
                if (!d->m_inputQueue.isEmpty()) {
                    QPair<AVFrame *, int64_t> frame;
                    {
                        QMutexLocker lock(&d->m_inputQueueMutex);
                        frame = d->m_inputQueue.dequeue();
                    }
                    if (frame.first->hw_frames_ctx) {
                        AVFrame *outFrame;
                        switch (frame.first->format) {
                            case AV_PIX_FMT_VAAPI:
                            case AV_PIX_FMT_DRM_PRIME:
                                qDebug("[AVQt::EncoderQSV] Mapping VAAPI/DRM_PRIME frame to QSV");
                                outFrame = av_frame_alloc();
                                outFrame->hw_frames_ctx = av_buffer_ref(d->m_pFramesCtx);
                                outFrame->format = AV_PIX_FMT_QSV;
                                av_hwframe_map(outFrame, frame.first, AV_HWFRAME_MAP_READ);
                                break;
                            case AV_PIX_FMT_QSV:
                                outFrame = av_frame_clone(frame.first);
                                break;

                        }
                        outFrame->pts = av_rescale_q(frame.first->pts, av_make_q(1, 1000000),
                                                     d->m_pCodecCtx->time_base); // Incoming timestamps are always microseconds
                        ret = avcodec_send_frame(d->m_pCodecCtx, outFrame);
                        qDebug("[AVQt::EncoderQSV] Sent frame with PTS %lld to encoder", static_cast<long long>(outFrame->pts));
                        av_frame_free(&frame.first);
                        av_frame_free(&outFrame);
                    } else {
                        av_hwframe_transfer_data(d->m_pHWFrame, frame.first, 0);
                        d->m_pHWFrame->pts = av_rescale_q(frame.first->pts, av_make_q(1, 1000000),
                                                          d->m_pCodecCtx->time_base); // Incoming timestamps are always microseconds
                        av_frame_free(&frame.first);
                        ret = avcodec_send_frame(d->m_pCodecCtx, d->m_pHWFrame);
                        qDebug("[AVQt::EncoderQSV] Sent frame with PTS %lld to encoder", static_cast<long long>(d->m_pHWFrame->pts));
                    }
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret != 0) {
                        qFatal("[AVQt::EncoderQSV] %i: Could not send frame to QSV encoder: %s", ret,
                               av_make_error_string(strBuf, strBufSize, ret));
                    }
                }
                AVPacket *packet = av_packet_alloc();
                while (true) {
                    ret = avcodec_receive_packet(d->m_pCodecCtx, packet);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret != 0) {
                        qFatal("[AVQt::EncoderQSV] %i: Could not receive packet from encoder: %s", ret,
                               av_make_error_string(strBuf, strBufSize, ret));
                    }
                    packet->pos = -1;
                    av_packet_rescale_ts(packet, d->m_pCodecCtx->time_base, av_make_q(1, 1000000));
                    qDebug("[AVQt::EncoderQSV] Got packet from encoder with PTS: %lld, DTS: %lld, duration: %lld, timebase: %d/%d",
                           static_cast<long long>(packet->pts),
                           static_cast<long long>(packet->dts),
                           static_cast<long long>(packet->duration),
                           d->m_pCodecCtx->time_base.num, d->m_pCodecCtx->time_base.den);
                    if (packet->buf) {
                        QMutexLocker lock(&d->m_cbListMutex);
                        for (const auto &cb: d->m_cbList) {
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

    EncoderQSV &EncoderQSV::operator=(EncoderQSV &&other) noexcept {
        delete d_ptr;
        d_ptr = other.d_ptr;
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;

        return *this;
    }
}