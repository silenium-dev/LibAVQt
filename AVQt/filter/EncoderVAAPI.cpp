//
// Created by silas on 4/18/21.
//

#include "private/EncoderVAAPI_p.h"
#include "EncoderVAAPI.h"

#include <QtCore>
#include "output/IPacketSink.h"

namespace AVQt {
    EncoderVAAPI::EncoderVAAPI(QString encoder, QObject *parent) : QThread(parent), d_ptr(new EncoderVAAPIPrivate(this)) {
        Q_D(AVQt::EncoderVAAPI);

        d->m_encoder = std::move(encoder);
    }

    EncoderVAAPI::EncoderVAAPI(EncoderVAAPIPrivate &p) : d_ptr(&p) {

    }

    EncoderVAAPI::~EncoderVAAPI() {
        delete d_ptr;
    }

    int EncoderVAAPI::init() {
        Q_D(AVQt::EncoderVAAPI);

//        int ret = 0;
//        constexpr auto strBufSize = 64;
//        char strBuf[strBufSize];

        d->m_pCodec = avcodec_find_encoder_by_name(d->m_encoder.toLocal8Bit().constData());
        if (!d->m_pCodec) {
            qFatal("Could not find encoder: %s", d->m_encoder.toLocal8Bit().constData());
        }

        return 0;
    }

    int EncoderVAAPI::deinit() {
        Q_D(AVQt::EncoderVAAPI);

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
        if (d->m_running.compare_exchange_strong(shouldBe, true)) {
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
        Q_UNUSED(source);
        Q_UNUSED(duration)
        Q_D(AVQt::EncoderVAAPI);
        d->m_framerate = framerate;
        init();
        return 0;
    }

    int EncoderVAAPI::deinit(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::EncoderVAAPI);
        deinit();
        return 0;
    }

    int EncoderVAAPI::start(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::EncoderVAAPI);
        start();
        return 0;
    }

    int EncoderVAAPI::stop(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::EncoderVAAPI);
        stop();
        return 0;
    }

    void EncoderVAAPI::pause(IFrameSource *source, bool paused) {
        Q_UNUSED(source)
        Q_D(AVQt::EncoderVAAPI);
        this->pause(paused);
    }

    qsizetype EncoderVAAPI::registerCallback(IPacketSink *packetSink, uint8_t type) {
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

    qsizetype EncoderVAAPI::unregisterCallback(IPacketSink *packetSink) {
        Q_D(AVQt::EncoderVAAPI);

        {
            QMutexLocker lock{&d->m_cbListMutex};
            auto count = d->m_cbList.removeAll(packetSink);
            return count > 0 ? count : -1;
        }
    }

    void EncoderVAAPI::onFrame(IFrameSource *source, AVFrame *frame, int64_t frameDuration) {
        Q_UNUSED(source);
        Q_D(AVQt::EncoderVAAPI);

        QPair<AVFrame *, int64_t> queueFrame{av_frame_alloc(), frameDuration};
        switch (frame->format) {
            case AV_PIX_FMT_VAAPI:
            case AV_PIX_FMT_DRM_PRIME:
            case AV_PIX_FMT_DXVA2_VLD:
                qDebug("Transferring frame from GPU to CPU");
                av_hwframe_transfer_data(queueFrame.first, frame, 0);
                break;
            default:
                qDebug("Referencing frame");
                av_frame_ref(queueFrame.first, frame);
                break;
        }

        while (d->m_inputQueue.size() > 100) {
            QThread::msleep(3);
        }
        {
            QMutexLocker lock{&d->m_inputQueueMutex};
            d->m_inputQueue.enqueue(queueFrame);
        }
    }

    void EncoderVAAPI::run() {
        Q_D(AVQt::EncoderVAAPI);

        int ret{0};
        constexpr auto strBufSize{64};
        char strBuf[strBufSize];

        while (d->m_running.load()) {
            if (!d->m_paused.load() && !d->m_inputQueue.isEmpty()) {
                bool shouldBe = true;
                if (d->m_firstFrame.compare_exchange_strong(shouldBe, false)) {
                    auto frame = d->m_inputQueue.first().first;

                    d->m_pCodecCtx = avcodec_alloc_context3(d->m_pCodec);
                    if (!d->m_pCodecCtx) {
                        qFatal("Could not create VAAPI encoder context");
                    }
                    d->m_pCodecCtx->width = frame->width;
                    d->m_pCodecCtx->height = frame->height;
//                    if (frame->hw_frames_ctx) {
//                        qDebug("Creating derived HW context...");
//                        d->m_pDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
//                        ret = av_hwdevice_ctx_create(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
//                        if (ret < 0) {
//                            qFatal("%i: Unable to create derived AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
//                        } else if (!d->m_pDeviceCtx) {
//                            qFatal("Unable to create derived AVHWDeviceContext");
//                        }
//
//                        d->m_pCodecCtx->sw_pix_fmt = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data)->sw_format;
//                        ret = av_hwframe_ctx_create_derived(&d->m_pFramesCtx, d->m_pCodecCtx->sw_pix_fmt, d->m_pDeviceCtx, frame->hw_frames_ctx, 0);
//                        if (ret < 0) {
//                            qFatal("%i: Unable to create derived AVHWFramesContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
//                        } else if (!d->m_pDeviceCtx) {
//                            qFatal("Unable to create derived AVHWFramesContext");
//                        }
//                        d->m_pCodecCtx->pix_fmt = reinterpret_cast<AVHWFramesContext*>(d->m_pFramesCtx->data)->format;
//                        d->m_pCodecCtx->hw_device_ctx = av_buffer_ref(d->m_pDeviceCtx);
//                        d->m_pCodecCtx->hw_frames_ctx = av_buffer_ref(d->m_pFramesCtx);
//                    } else {
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
                    framesContext->initial_pool_size = 20;

                    ret = av_hwframe_ctx_init(d->m_pFramesCtx);
                    if (ret != 0) {
                        qFatal("%i: Could not init HW frames context: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    d->m_pCodecCtx->pix_fmt = framesContext->format;

                    d->m_pCodecCtx->hw_device_ctx = av_buffer_ref(d->m_pDeviceCtx);
                    d->m_pCodecCtx->hw_frames_ctx = av_buffer_ref(d->m_pFramesCtx);
//                    }

//                    d->m_pCodecCtx->pix_fmt = AV_PIX_FMT_VAAPI;
                    d->m_pCodecCtx->time_base = av_inv_q(d->m_framerate);
                    ret = avcodec_open2(d->m_pCodecCtx, d->m_pCodec, nullptr);
                    if (ret < 0) {
                        qFatal("%i: Unable to open VAAPI encoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                    d->m_pHWFrame = av_frame_alloc();
                    ret = av_hwframe_get_buffer(d->m_pFramesCtx, d->m_pHWFrame, 0);
                    if (ret != 0) {
                        qFatal("%i: Could not allocate HW frame in encoder context: %s", ret,
                               av_make_error_string(strBuf, strBufSize, ret));
                    }
                }
                while (!d->m_inputQueue.isEmpty()) {
                    QPair<AVFrame *, int64_t> frame;
                    {
                        QMutexLocker lock(&d->m_inputQueueMutex);
                        frame = d->m_inputQueue.dequeue();
                    }
                    if (frame.first->hw_frames_ctx) {
                        av_hwframe_map(d->m_pHWFrame, frame.first, AV_HWFRAME_MAP_READ);
                    } else {
                        av_hwframe_transfer_data(d->m_pHWFrame, frame.first, 0);
                    }
                    av_frame_free(&frame.first);
                    ret = avcodec_send_frame(d->m_pCodecCtx, d->m_pHWFrame);
                    if (ret != 0) {
                        qFatal("%i: Could not send frame to VAAPI encoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                }
                AVPacket *packet = av_packet_alloc();
                while (true) {
                    ret = avcodec_receive_packet(d->m_pCodecCtx, packet);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret < 0) {
                        qFatal("%i: Could not receive packet from encoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                    {
                        QMutexLocker lock(&d->m_cbListMutex);
                        for (const auto &cb: d->m_cbList) {
                            AVPacket *cbPacket = av_packet_clone(packet);
                            cb->onPacket(this, cbPacket, IPacketSource::CB_VIDEO);
                            av_packet_free(&cbPacket);
                        }
                    }
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

        return *this;
    }

    EncoderVAAPI::EncoderVAAPI(EncoderVAAPI &&other) {
        d_ptr = other.d_ptr;
        other.d_ptr = nullptr;
    }
}