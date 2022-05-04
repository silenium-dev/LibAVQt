// Copyright (c) 2021.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#define AVQT_INCLUDE_INTERNAL
#include "DecoderDXVA2.hpp"
#undef AVQT_INCLUDE_INTERNAL

#include "AVQt/decoder/private/DecoderDXVA2_p.hpp"
#include "AVQt/input/IPacketSource.hpp"
#include "output/IAudioSink.hpp"
#include "output/IFrameSink.hpp"

#include <QApplication>


namespace AVQt {
    DecoderDXVA2::DecoderDXVA2(QObject *parent) : QThread(parent), d_ptr(new DecoderDXVA2Private(this)) {
    }

    [[maybe_unused]] DecoderDXVA2::DecoderDXVA2(DecoderDXVA2Private &p) : d_ptr(&p) {
    }

    DecoderDXVA2::DecoderDXVA2(DecoderDXVA2 &&other) noexcept: d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    DecoderDXVA2::~DecoderDXVA2() {
        delete d_ptr;
    }

    void DecoderDXVA2::init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                            AVCodecParameters *aParams, AVCodecParameters *sParams) {
        Q_UNUSED(aParams)
        Q_UNUSED(sParams)
        Q_D(AVQt::DecoderDXVA2);
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

    int DecoderDXVA2::init() {
        return 0;
    }

    void DecoderDXVA2::deinit(IPacketSource *source) {
        Q_UNUSED(source)
        deinit();
    }

    int DecoderDXVA2::deinit() {
        Q_D(AVQt::DecoderDXVA2);

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

    void DecoderDXVA2::start(IPacketSource *source) {
        Q_UNUSED(source)
        start();
    }

    int DecoderDXVA2::start() {
        Q_D(AVQt::DecoderDXVA2);

        bool notRunning = false;
        if (d->m_running.compare_exchange_strong(notRunning, true)) {
            d->m_paused.store(false);
            paused(false);

            QThread::start();
            return 0;
        }
        return -1;
    }

    void DecoderDXVA2::stop(IPacketSource *source) {
        Q_UNUSED(source)
        stop();
    }

    int DecoderDXVA2::stop() {
        Q_D(AVQt::DecoderDXVA2);

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

    void DecoderDXVA2::pause(bool pause) {
        Q_D(AVQt::DecoderDXVA2);
        if (d->m_running.load() != pause) {
            d->m_paused.store(pause);

            for (const auto &cb: d->m_cbList) {
                cb->pause(this, pause);
            }

            paused(pause);
        }
    }

    bool DecoderDXVA2::isPaused() {
        Q_D(AVQt::DecoderDXVA2);
        return d->m_paused.load();
    }

    qint64 DecoderDXVA2::registerCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderDXVA2);

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

    qint64 DecoderDXVA2::unregisterCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderDXVA2);
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

    void DecoderDXVA2::onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) {
        Q_UNUSED(source)

        Q_D(AVQt::DecoderDXVA2);

        if (packetType == IPacketSource::CB_VIDEO) {
            AVPacket *queuePacket = av_packet_clone(packet);
            while (d->m_inputQueue.size() >= 50) {
                QThread::msleep(4);
            }
            QMutexLocker lock(&d->m_inputQueueMutex);
            d->m_inputQueue.append(queuePacket);
        }
    }

    void DecoderDXVA2::run() {
        Q_D(AVQt::DecoderDXVA2);

        while (d->m_running) {
            if (!d->m_paused.load() && !d->m_inputQueue.isEmpty()) {
                int ret;
                constexpr size_t strBufSize = 64;
                char strBuf[strBufSize];
                // If m_pCodecParams is nullptr, it is not initialized by packet source, if video codec context is nullptr, this is the first packet
                if (d->m_pCodecParams && !d->m_pCodecCtx) {
                    d->m_pCodec = avcodec_find_decoder(d->m_pCodecParams->codec_id);
                    if (!d->m_pCodec) {
                        qFatal("No audio decoder found");
                    }

                    d->m_pCodecCtx = avcodec_alloc_context3(d->m_pCodec);
                    if (!d->m_pCodecCtx) {
                        qFatal("Could not allocate audio decoder context, probably out of memory");
                    }

                    ret = av_hwdevice_ctx_create(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_DXVA2, "", nullptr, 0);
                    if (ret != 0) {
                        qFatal("%d: Could not create AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    avcodec_parameters_to_context(d->m_pCodecCtx, d->m_pCodecParams);
                    d->m_pCodecCtx->hw_device_ctx = av_buffer_ref(d->m_pDeviceCtx);
                    ret = avcodec_open2(d->m_pCodecCtx, d->m_pCodec, nullptr);
                    if (ret != 0) {
                        qFatal("%d: Could not open DXVA2 decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
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
                            qFatal("%d: Error sending packet to DXVA2 decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
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
                        qFatal("%d: Error receiving frame from DXVA2 decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
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
                        cb->onFrame(this, cbFrame, static_cast<int64_t>(av_q2d(av_inv_q(d->m_framerate)) * 1000.0), d->m_pDeviceCtx);
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
