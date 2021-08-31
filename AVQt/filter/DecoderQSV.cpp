//
// Created by silas on 3/1/21.
//

#include "private/DecoderQSV_p.h"
#include "DecoderQSV.h"
#include "output/IFrameSink.h"
#include "output/IAudioSink.h"
#include "input/IPacketSource.h"

#include <QApplication>
#include <QImage>
//#include <QtConcurrent>

//#ifndef DOXYGEN_SHOULD_SKIP_THIS
//#define NOW() std::chrono::high_resolution_clock::now()
//#define TIME_US(t1, t2) std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()
//#endif


namespace AVQt {
    DecoderQSV::DecoderQSV(QObject *parent) : QThread(parent), d_ptr(new DecoderQSVPrivate(this)) {
    }

    [[maybe_unused]] DecoderQSV::DecoderQSV(DecoderQSVPrivate &p) : d_ptr(&p) {

    }

    DecoderQSV::DecoderQSV(DecoderQSV &&other) noexcept: d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    DecoderQSV::~DecoderQSV() {
        delete d_ptr;
    }

    void DecoderQSV::init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                          AVCodecParameters *aParams, AVCodecParameters *sParams) {
        Q_UNUSED(aParams)
        Q_UNUSED(sParams)
        Q_D(AVQt::DecoderQSV);
        Q_UNUSED(source)
        if (d->m_pCodecParams) {
            avcodec_parameters_free(&d->m_pCodecParams);
        }
        d->m_pCodecParams = avcodec_parameters_alloc();
        avcodec_parameters_copy(d->m_pCodecParams, vParams);
        d->m_duration = duration;
        d->m_framerate = framerate;
        d->m_timebase = timebase;
        {
            QMutexLocker lock(&d->m_cbListMutex);
            for (const auto &cb: d->m_cbList) {
                cb->init(this, framerate, duration);
            }
        }
        init();
    }

    int DecoderQSV::init() {
        return 0;
    }

    void DecoderQSV::deinit(IPacketSource *source) {
        Q_UNUSED(source)
        deinit();
    }

    int DecoderQSV::deinit() {
        Q_D(AVQt::DecoderQSV);

        stop();

        if (d->m_pCodecParams) {
            avcodec_parameters_free(&d->m_pCodecParams);
            d->m_pCodecParams = nullptr;
        }
        if (d->m_pCodecCtx) {
            avcodec_close(d->m_pCodecCtx);
            avcodec_free_context(&d->m_pCodecCtx);
            d->m_pCodecCtx = nullptr;
            d->m_pCodec = nullptr;
            av_buffer_unref(&d->m_pDeviceCtx);
            d->m_pDeviceCtx = nullptr;
        }

        return 0;
    }

    void DecoderQSV::start(IPacketSource *source) {
        Q_UNUSED(source)
        start();
    }

    int DecoderQSV::start() {
        Q_D(AVQt::DecoderQSV);

        bool notRunning = false;
        if (d->m_running.compare_exchange_strong(notRunning, true)) {
            d->m_paused.store(false);
            paused(false);

            QThread::start();
            return 0;
        }
        return -1;
    }

    void DecoderQSV::stop(IPacketSource *source) {
        Q_UNUSED(source)
        stop();
    }

    int DecoderQSV::stop() {
        Q_D(AVQt::DecoderQSV);

        bool shouldBeCurrent = true;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, false)) {
            {
//                QMutexLocker lock(&d->m_cbListMutex);
                for (const auto &cb: d->m_cbList) {
                    cb->stop(this);
                }
            }
            {
                QMutexLocker lock(&d->m_inputQueueMutex);
                for (auto &packet : d->m_inputQueue) {
                    av_packet_unref(packet);
                    av_packet_free(&packet);
                }
                d->m_inputQueue.clear();
            }

            wait();

            return 0;
        }

        return -1;
    }

    void DecoderQSV::pause(bool pause) {
        Q_D(AVQt::DecoderQSV);
        if (d->m_running.load() != pause) {
            d->m_paused.store(pause);

            for (const auto &cb: d->m_cbList) {
                cb->pause(this, pause);
            }

            paused(pause);
        }
    }

    bool DecoderQSV::isPaused() {
        Q_D(AVQt::DecoderQSV);
        return d->m_paused.load();
    }

    qint64 DecoderQSV::registerCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderQSV);

        QMutexLocker lock(&d->m_cbListMutex);
        if (!d->m_cbList.contains(frameSink)) {
            d->m_cbList.append(frameSink);
            if (d->m_running.load() && d->m_pCodecCtx) {
                frameSink->init(this, d->m_framerate, d->m_duration);
                frameSink->start(this);
            }
            return d->m_cbList.indexOf(frameSink);
        }
        return -1;
    }

    qint64 DecoderQSV::unregisterCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderQSV);
        QMutexLocker lock(&d->m_cbListMutex);
        if (d->m_cbList.contains(frameSink)) {
            auto result = d->m_cbList.indexOf(frameSink);
            d->m_cbList.removeOne(frameSink);
            frameSink->stop(this);
            frameSink->deinit(this);
            return result;
        }
        return -1;
    }

    void DecoderQSV::onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) {
        Q_UNUSED(source)

        Q_D(AVQt::DecoderQSV);

        if (packetType == IPacketSource::CB_VIDEO) {
            AVPacket *queuePacket = av_packet_clone(packet);
            while (d->m_inputQueue.size() >= 50) {
                QThread::msleep(4);
            }
            QMutexLocker lock(&d->m_inputQueueMutex);
            d->m_inputQueue.append(queuePacket);
        }
    }

    void DecoderQSV::run() {
        Q_D(AVQt::DecoderQSV);

        while (d->m_running) {
            if (!d->m_paused.load() && !d->m_inputQueue.isEmpty()) {
                int ret;
                constexpr size_t strBufSize = 64;
                char strBuf[strBufSize];
                // If m_pCodecParams is nullptr, it is not initialized by packet source, if video codec context is nullptr, this is the first packet
                if (d->m_pCodecParams && !d->m_pCodecCtx) {
                    switch (d->m_pCodecParams->codec_id) {
                        case AV_CODEC_ID_H264:
                            d->m_pCodec = avcodec_find_decoder_by_name("h264_qsv");
                            break;
                        case AV_CODEC_ID_HEVC:
                            d->m_pCodec = avcodec_find_decoder_by_name("hevc_qsv");
                            break;
                        case AV_CODEC_ID_VP8:
                            d->m_pCodec = avcodec_find_decoder_by_name("vp8_qsv");
                            break;
                        case AV_CODEC_ID_VP9:
                            d->m_pCodec = avcodec_find_decoder_by_name("vp9_qsv");
                            break;
                        case AV_CODEC_ID_VC1:
                            d->m_pCodec = avcodec_find_decoder_by_name("vc1_qsv");
                            break;
                        case AV_CODEC_ID_AV1:
                            d->m_pCodec = avcodec_find_decoder_by_name("av1_qsv");
                            break;
                        case AV_CODEC_ID_MPEG2VIDEO:
                            d->m_pCodec = avcodec_find_decoder_by_name("mpeg2_qsv");
                            break;
                        case AV_CODEC_ID_MJPEG:
                            d->m_pCodec = avcodec_find_decoder_by_name("mjpeg_qsv");
                            break;
                        default:
                            qFatal("[AVQt::DecoderQSV] Unsupported codec: %s", av_fourcc_make_string(strBuf, d->m_pCodecParams->codec_id));
                    }

                    if (!d->m_pCodec) {
                        qFatal("No video decoder found");
                    }

                    d->m_pCodecCtx = avcodec_alloc_context3(d->m_pCodec);
                    if (!d->m_pCodecCtx) {
                        qFatal("Could not allocate audio decoder context, probably out of memory");
                    }

                    ret = av_hwdevice_ctx_create(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_QSV, "/dev/dri/renderD128", nullptr, 0);
                    if (ret != 0) {
                        qFatal("%d: Could not create AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    avcodec_parameters_to_context(d->m_pCodecCtx, d->m_pCodecParams);
                    d->m_pCodecCtx->hw_device_ctx = av_buffer_ref(d->m_pDeviceCtx);
                    d->m_pCodecCtx->get_format = &DecoderQSVPrivate::getFormat;
                    ret = avcodec_open2(d->m_pCodecCtx, d->m_pCodec, nullptr);
                    if (ret != 0) {
                        qFatal("%d: Could not open QSV decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    for (const auto &cb: d->m_cbList) {
                        cb->start(this);
                    }
                    started();
                }
                {
                    QMutexLocker lock(&d->m_inputQueueMutex);
                    while (!d->m_inputQueue.isEmpty()) {
                        AVPacket *packet = d->m_inputQueue.dequeue();
                        lock.unlock();

                        qDebug("Video packet queue size: %d", d->m_inputQueue.size());

                        ret = avcodec_send_packet(d->m_pCodecCtx, packet);
                        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                            lock.relock();
                            d->m_inputQueue.prepend(packet);
                            break;
                        } else if (ret < 0) {
                            qFatal("%d: Error sending packet to QSV decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                        }
                        av_packet_unref(packet);
                        av_packet_free(&packet);
                        lock.relock();
                    }
                }
                AVFrame *frame = av_frame_alloc();
                while (true) {
                    ret = avcodec_receive_frame(d->m_pCodecCtx, frame);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret < 0) {
                        qFatal("%d: Error receiving frame from QSV decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

//                    auto t1 = NOW();
//                    AVFrame *swFrame = av_frame_alloc();
//                    ret = av_hwframe_transfer_data(swFrame, frame, 0);
//                    if (ret != 0) {
//                        qFatal("%d: Could not get hw frame: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
//                    }
//                    auto t3 = NOW();

                    QMutexLocker lock2(&d->m_cbListMutex);
//                    QList<QFuture<void>> cbFutures;
                    for (const auto &cb: d->m_cbList) {
//                        cbFutures.append(QtConcurrent::run([=] {
                        AVFrame *cbFrame = av_frame_clone(frame);
                        cbFrame->pts = av_rescale_q(frame->pts, d->m_timebase,
                                                    av_make_q(1, 1000000)); // Rescale pts to microseconds for easier processing
                        qDebug("Calling video frame callback for PTS: %lld, Timebase: %d/%d", static_cast<long long>(cbFrame->pts),
                               d->m_timebase.num,
                               d->m_timebase.den);
                        QTime time = QTime::currentTime();
                        cb->onFrame(this, cbFrame, static_cast<int64_t>(av_q2d(av_inv_q(d->m_framerate)) * 1000.0),
                                    av_buffer_ref(d->m_pDeviceCtx));
                        qDebug() << "Video CB time:" << time.msecsTo(QTime::currentTime());
                        av_frame_free(&cbFrame);
//                        }));
                    }
//                    bool cbBusy = true;
//                    while (cbBusy) {
//                        cbBusy = false;
//                        for (const auto &future: cbFutures) {
//                            if (future.isRunning()) {
//                                cbBusy = true;
//                                break;
//                            }
//                        }
//                        usleep(500);
//                    }
//                    auto t2 = NOW();
//                    qDebug("Decoder frame transfer time: %ld us", TIME_US(t1, t3));

//                    av_frame_free(&swFrame);
                    av_frame_unref(frame);
                }
                av_frame_free(&frame);
            } else {
                msleep(4);
            }
        }
    }

#include <libavutil/hwcontext_qsv.h>

    AVPixelFormat DecoderQSVPrivate::getFormat(AVCodecContext *pCodecCtx, const enum AVPixelFormat *pix_fmts) {
        while (*pix_fmts != AV_PIX_FMT_NONE) {
            if (*pix_fmts == AV_PIX_FMT_QSV) {
                AVHWFramesContext *frames_ctx;
                AVQSVFramesContext *frames_hwctx;
                int ret;

                /* create a pool of surfaces to be used by the decoder */
                pCodecCtx->hw_frames_ctx = av_hwframe_ctx_alloc(pCodecCtx->hw_device_ctx);
                if (!pCodecCtx->hw_frames_ctx) {
                    return AV_PIX_FMT_NONE;
                }
                frames_ctx = (AVHWFramesContext *) pCodecCtx->hw_frames_ctx->data;
                frames_hwctx = static_cast<AVQSVFramesContext *>(frames_ctx->hwctx);

                frames_ctx->format = AV_PIX_FMT_QSV;
                frames_ctx->sw_format = pCodecCtx->sw_pix_fmt;
                frames_ctx->width = FFALIGN(pCodecCtx->coded_width, 32);
                frames_ctx->height = FFALIGN(pCodecCtx->coded_height, 32);
                frames_ctx->initial_pool_size = 32;

                frames_hwctx->frame_type = 0x00100; // 0x00100 is equivalent to MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET, but additional headers would be required to use this constant

                ret = av_hwframe_ctx_init(pCodecCtx->hw_frames_ctx);
                if (ret < 0)
                    return AV_PIX_FMT_NONE;

                return AV_PIX_FMT_QSV;
            }

            pix_fmts++;
        }

        fprintf(stderr, "The QSV pixel format not offered in get_format()\n");

        return AV_PIX_FMT_NONE;
    }
}
