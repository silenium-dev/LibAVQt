//
// Created by silas on 3/1/21.
//

#include "private/DecoderMMAL_p.h"
#include "DecoderMMAL.h"
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
    DecoderMMAL::DecoderMMAL(QObject *parent) : QThread(parent), d_ptr(new DecoderMMALPrivate(this)) {
    }

    [[maybe_unused]] DecoderMMAL::DecoderMMAL(DecoderMMALPrivate &p) : d_ptr(&p) {

    }

    DecoderMMAL::DecoderMMAL(DecoderMMAL &&other) noexcept: d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    DecoderMMAL::~DecoderMMAL() {
        delete d_ptr;
    }

    void DecoderMMAL::init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                           AVCodecParameters *aParams, AVCodecParameters *sParams) {
        Q_UNUSED(aParams)
        Q_UNUSED(sParams)
        Q_D(AVQt::DecoderMMAL);
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

    int DecoderMMAL::init() {
        return 0;
    }

    void DecoderMMAL::deinit(IPacketSource *source) {
        Q_UNUSED(source)
        deinit();
    }

    int DecoderMMAL::deinit() {
        Q_D(AVQt::DecoderMMAL);

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
        }

        return 0;
    }

    void DecoderMMAL::start(IPacketSource *source) {
        Q_UNUSED(source)
        start();
    }

    int DecoderMMAL::start() {
        Q_D(AVQt::DecoderMMAL);

        bool notRunning = false;
        if (d->m_running.compare_exchange_strong(notRunning, true)) {
            d->m_paused.store(false);
            paused(false);

            QThread::start();
            return 0;
        }
        return -1;
    }

    void DecoderMMAL::stop(IPacketSource *source) {
        Q_UNUSED(source)
        stop();
    }

    int DecoderMMAL::stop() {
        Q_D(AVQt::DecoderMMAL);

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

    void DecoderMMAL::pause(bool pause) {
        Q_D(AVQt::DecoderMMAL);
        if (d->m_running.load() != pause) {
            d->m_paused.store(pause);

            for (const auto &cb: d->m_cbList) {
                cb->pause(this, pause);
            }

            paused(pause);
        }
    }

    bool DecoderMMAL::isPaused() {
        Q_D(AVQt::DecoderMMAL);
        return d->m_paused.load();
    }

    qint64 DecoderMMAL::registerCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderMMAL);

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

    qint64 DecoderMMAL::unregisterCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderMMAL);
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

    void DecoderMMAL::onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) {
        Q_UNUSED(source)

        Q_D(AVQt::DecoderMMAL);

        if (packetType == IPacketSource::CB_VIDEO) {
            AVPacket *queuePacket = av_packet_clone(packet);
            while (d->m_inputQueue.size() >= 50) {
                QThread::msleep(4);
            }
            QMutexLocker lock(&d->m_inputQueueMutex);
            d->m_inputQueue.append(queuePacket);
        }
    }

    void DecoderMMAL::run() {
        Q_D(AVQt::DecoderMMAL);

        while (d->m_running) {
            if (!d->m_paused.load() && !d->m_inputQueue.isEmpty()) {
                int ret;
                constexpr size_t strBufSize = 64;
                char strBuf[strBufSize];
                // If m_pCodecParams is nullptr, it is not initialized by packet source, if video codec context is nullptr, this is the first packet
                if (d->m_pCodecParams && !d->m_pCodecCtx) {
                    switch (d->m_pCodecParams->codec_id) {
                        case AV_CODEC_ID_H264:
                            d->m_pCodec = avcodec_find_decoder_by_name("h264_mmal");
                            break;
                        case AV_CODEC_ID_MPEG2VIDEO:
                            d->m_pCodec = avcodec_find_decoder_by_name("mpeg2_mmal");
                            break;
                        case AV_CODEC_ID_MPEG4:
                            d->m_pCodec = avcodec_find_decoder_by_name("mpeg4_mmal");
                            break;
                        case AV_CODEC_ID_VC1:
                            d->m_pCodec = avcodec_find_decoder_by_name("vc1_mmal");
                            break;
                        default:
                            qFatal("No MMAL decoder found for the specified codec: %s", avcodec_get_name(d->m_pCodecParams->codec_id));
                            break;
                    }

                    d->m_pCodecCtx = avcodec_alloc_context3(d->m_pCodec);
                    if (!d->m_pCodecCtx) {
                        qFatal("Could not allocate MMAL decoder context, probably out of memory");
                    }

                    avcodec_parameters_to_context(d->m_pCodecCtx, d->m_pCodecParams);
                    ret = avcodec_open2(d->m_pCodecCtx, d->m_pCodec, nullptr);
                    if (ret != 0) {
                        qFatal("%d: Could not open MMAL decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
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
                            qFatal("%d: Error sending packet to MMAL decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
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
                        qFatal("%d: Error receiving frame from MMAL decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
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
                        cb->onFrame(this, cbFrame, static_cast<int64_t>(av_q2d(av_inv_q(d->m_framerate)) * 1000.0), nullptr);
                        qDebug() << "Video CB time:" << time.msecsTo(QTime::currentTime());
                        av_frame_unref(cbFrame);
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
}