//
// Created by silas on 3/1/21.
//

#include "private/DecoderVAAPI_p.h"
#include "DecoderVAAPI.h"
#include "../output/IFrameSink.h"
#include "../output/IAudioSink.h"
#include "IPacketSource.h"

#include <QApplication>
#include <QImage>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define NOW() std::chrono::high_resolution_clock::now()
#define TIME_US(t1, t2) std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()
#endif

void ffmpeg_dump_yuv(const char *filename, AVFrame *avFrame) {
    FILE *fDump = fopen(filename, "ab");

    uint32_t pitchY = avFrame->linesize[0];
    uint32_t pitchU = avFrame->linesize[1];
    uint32_t pitchV = avFrame->linesize[2];

    uint8_t *avY = avFrame->data[0];
    uint8_t *avU = avFrame->data[1];
    uint8_t *avV = avFrame->data[2];

    for (uint32_t i = 0; i < avFrame->height; i++) {
        fwrite(avY, avFrame->width, 1, fDump);
        avY += pitchY;
    }

    for (uint32_t i = 0; i < avFrame->height / 2; i++) {
        fwrite(avU, avFrame->width / 2, 1, fDump);
        avU += pitchU;
    }

    for (uint32_t i = 0; i < avFrame->height / 2; i++) {
        fwrite(avV, avFrame->width / 2, 1, fDump);
        avV += pitchV;
    }

    fclose(fDump);
}

namespace AVQt {
    DecoderVAAPI::DecoderVAAPI(QObject *parent) : QThread(parent), d_ptr(new DecoderVAAPIPrivate(this)) {
        Q_D(AVQt::DecoderVAAPI);
    }

    [[maybe_unused]] DecoderVAAPI::DecoderVAAPI(DecoderVAAPIPrivate &p) : d_ptr(&p) {

    }

    DecoderVAAPI::~DecoderVAAPI() {
        delete d_ptr;
        d_ptr = nullptr;
    }

    int DecoderVAAPI::init() {
        Q_D(AVQt::DecoderVAAPI);

        return 0;
    }

    int DecoderVAAPI::deinit() {
        Q_D(AVQt::DecoderVAAPI);

        if (d->m_pCodecParams) {
            avcodec_parameters_free(&d->m_pCodecParams);
            d->m_pCodecParams = nullptr;
        }
        if (d->m_pVideoCodecCtx) {
            avcodec_close(d->m_pVideoCodecCtx);
            avcodec_free_context(&d->m_pVideoCodecCtx);
            d->m_pVideoCodecCtx = nullptr;
            d->m_pVideoCodec = nullptr;
            av_buffer_unref(&d->m_pDeviceCtx);
            d->m_pDeviceCtx = nullptr;
        }

        return 0;
    }

    int DecoderVAAPI::start() {
        Q_D(AVQt::DecoderVAAPI);
        bool notRunning = false;
        if (d->m_running.compare_exchange_strong(notRunning, true)) {
            d->m_paused.store(false);
            paused(false);

            QThread::start();
            started();
            return 0;
        }
        return -1;
    }

    int DecoderVAAPI::stop() {
        Q_D(AVQt::DecoderVAAPI);
        if (d->m_running) {
            d->m_running.store(false);

            for (const auto &cb: d->m_cbList) {
                cb->stop(this);
            }

            wait();
//            qDebug() << "DecoderVAAPI stopped";
            return 0;
        } else {
            return -1;
        }
    }

    void DecoderVAAPI::pause(bool pause) {
        Q_D(AVQt::DecoderVAAPI);
        if (d->m_running.load() != pause) {
            d->m_paused.store(pause);

            for (const auto &cb: d->m_cbList) {
                cb->pause(this, pause);
            }

            paused(pause);
        }
    }

    bool DecoderVAAPI::isPaused() {
        Q_D(AVQt::DecoderVAAPI);
        return d->m_paused.load();
    }

    int DecoderVAAPI::registerCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderVAAPI);

        QMutexLocker lock(&d->m_cbListMutex);
        if (!d->m_cbList.contains(frameSink)) {
            d->m_cbList.append(frameSink);
            return d->m_cbList.indexOf(frameSink);
        }
        return -1;
    }

    int DecoderVAAPI::unregisterCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderVAAPI);
        QMutexLocker lock(&d->m_cbListMutex);
        if (d->m_cbList.contains(frameSink)) {
            int result = d->m_cbList.indexOf(frameSink);
            d->m_cbList.removeOne(frameSink);
            return result;
        }
        return -1;
    }

    void DecoderVAAPI::run() {
        Q_D(AVQt::DecoderVAAPI);

        while (d->m_running) {
            QMutexLocker lock(&d->m_inputQueueMutex);
            if (!d->m_paused.load() && !d->m_inputQueue.isEmpty()) {
                AVPacket *packet = d->m_inputQueue.dequeue();
                lock.unlock();
                int ret = 0;
                constexpr size_t strBufSize = 64;
                char strBuf[strBufSize];

                // If m_pCodecParams is nullptr, it is not initialized by packet source, if video codec context is nullptr, this is the first packet
                if (d->m_pCodecParams && !d->m_pVideoCodecCtx) {
                    d->m_pVideoCodec = avcodec_find_decoder(d->m_pCodecParams->codec_id);
                    d->m_pVideoCodecCtx = avcodec_alloc_context3(d->m_pVideoCodec);

                    ret = av_hwdevice_ctx_create(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
                    if (ret != 0) {
                        qFatal("%d: Could not create AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    avcodec_parameters_to_context(d->m_pVideoCodecCtx, d->m_pCodecParams);
                    d->m_pVideoCodecCtx->hw_device_ctx = av_buffer_ref(d->m_pDeviceCtx);
                    ret = avcodec_open2(d->m_pVideoCodecCtx, d->m_pVideoCodec, nullptr);
                    if (ret != 0) {
                        qFatal("%d: Could not open VAAPI decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    for (const auto &cb: d->m_cbList) {
                        cb->init(this, d->m_pVideoCodecCtx->time_base, d->m_pVideoCodecCtx->framerate, d->m_duration);
                        cb->start(this);
                    }
                }
                AVFrame *frame = av_frame_alloc();
                ret = avcodec_send_packet(d->m_pVideoCodecCtx, packet);
                if (ret < 0) {
                    qFatal("%d: Error sending packwt to VAAPI decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                } else {
                    qDebug("Sent packet to decoder");
                }
                while (true) {
                    ret = avcodec_receive_frame(d->m_pVideoCodecCtx, frame);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret < 0) {
                        qFatal("%d: Error receiving frame from VAAPI decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    AVFrame *swFrame = av_frame_alloc();
                    ret = av_hwframe_transfer_data(swFrame, frame, 0);
                    if (ret != 0) {
                        qFatal("%d: Could not get hw frame: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    QMutexLocker lock(&d->m_cbListMutex);
                    for (const auto &cb: d->m_cbList) {
                        qDebug("Calling frame callbacks");
                        AVFrame *cbFrame = av_frame_clone(swFrame);
                        cb->onFrame(this, cbFrame, av_q2d(av_inv_q(d->m_framerate)) * 1000.0);
                    }
                }
            } else {
                msleep(4);
            }
        }
    }

    void DecoderVAAPI::onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) {
        Q_UNUSED(source);

        Q_D(AVQt::DecoderVAAPI);

        if (packetType == IPacketSource::CB_VIDEO) {
            AVPacket *queuePacket = av_packet_clone(packet);
            while (d->m_cbList.size() >= 100) {
                QThread::msleep(1);
            }
            QMutexLocker lock(&d->m_inputQueueMutex);
            d->m_inputQueue.append(queuePacket);
        }
    }

    void DecoderVAAPI::init(IPacketSource *source, AVRational framerate, int64_t duration, AVCodecParameters *vParams,
                            AVCodecParameters *aParams,
                            AVCodecParameters *sParams) {
        Q_D(AVQt::DecoderVAAPI);
        Q_UNUSED(source)
        if (d->m_pCodecParams) {
            avcodec_parameters_free(&d->m_pCodecParams);
        }
        d->m_pCodecParams = avcodec_parameters_alloc();
        avcodec_parameters_copy(d->m_pCodecParams, vParams);
        d->m_duration = duration;
        d->m_framerate = framerate;
        init();
    }

    void DecoderVAAPI::deinit(IPacketSource *source) {
        Q_UNUSED(source)
        deinit();
    }

    void DecoderVAAPI::start(IPacketSource *source) {
        Q_UNUSED(source)
        start();
    }

    void DecoderVAAPI::stop(IPacketSource *source) {
        Q_UNUSED(source)
        stop();
    }
}
