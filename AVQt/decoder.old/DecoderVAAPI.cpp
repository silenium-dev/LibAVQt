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

#include "DecoderVAAPI.hpp"
#include "decoder/private/Decoder_p.hpp"
#include "input/IPacketSource.hpp"
#include "output/IAudioSink.hpp"
#include "output/IFrameSink.hpp"

#include <QApplication>
//#include <QtConcurrent>

//#ifndef DOXYGEN_SHOULD_SKIP_THIS
//#define NOW() std::chrono::high_resolution_clock::now()
//#define TIME_US(t1, t2) std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()
//#endif


namespace AVQt {
    DecoderVAAPI::DecoderVAAPI(QObject *parent) : QThread(parent), d_ptr(new DecoderVAAPIPrivate(this)) {
    }

    [[maybe_unused]] DecoderVAAPI::DecoderVAAPI(DecoderVAAPIPrivate &p) : d_ptr(&p) {
    }

    DecoderVAAPI::DecoderVAAPI(DecoderVAAPI &&other) noexcept : d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    DecoderVAAPI::~DecoderVAAPI() {
        delete d_ptr;
    }

    void DecoderVAAPI::init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                            AVCodecParameters *aParams, AVCodecParameters *sParams) {
        Q_UNUSED(aParams)
        Q_UNUSED(sParams)
        Q_D(AVQt::DecoderVAAPI);
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
            for (const auto &cb : d->m_cbList) {
                cb->init(this, framerate, duration);
            }
        }
        init();
    }

    int DecoderVAAPI::init() {
        return 0;
    }

    void DecoderVAAPI::deinit(IPacketSource *source) {
        Q_UNUSED(source)
        deinit();
    }

    int DecoderVAAPI::deinit() {
        Q_D(AVQt::DecoderVAAPI);

        stop();

        {
            QMutexLocker lock(&d->m_cbListMutex);
            for (const auto &cb : d->m_cbList) {
                cb->deinit(this);
            }
        }

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

    void DecoderVAAPI::start(IPacketSource *source) {
        Q_UNUSED(source)
        start();
    }

    int DecoderVAAPI::start() {
        Q_D(AVQt::DecoderVAAPI);

        bool notRunning = false;
        if (d->m_running.compare_exchange_strong(notRunning, true)) {
            d->m_paused.store(false);
            paused(false);

            QThread::start();
            return 0;
        }
        return -1;
    }

    void DecoderVAAPI::stop(IPacketSource *source) {
        Q_UNUSED(source)
        stop();
    }

    int DecoderVAAPI::stop() {
        Q_D(AVQt::DecoderVAAPI);

        bool shouldBeCurrent = true;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, false)) {
            {
                //                QMutexLocker lock(&d->m_cbListMutex);
                for (const auto &cb : d->m_cbList) {
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

    void DecoderVAAPI::pause(bool pause) {
        Q_D(AVQt::DecoderVAAPI);
        if (d->m_running.load() != pause) {
            d->m_paused.store(pause);

            for (const auto &cb : d->m_cbList) {
                cb->pause(this, pause);
            }

            paused(pause);
        }
    }

    bool DecoderVAAPI::isPaused() {
        Q_D(AVQt::DecoderVAAPI);
        return d->m_paused.load();
    }

    qint64 DecoderVAAPI::registerCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderVAAPI);

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

    qint64 DecoderVAAPI::unregisterCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderVAAPI);
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

    void DecoderVAAPI::onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) {
        Q_UNUSED(source)

        Q_D(AVQt::DecoderVAAPI);

        if (packetType == IPacketSource::CB_VIDEO) {
            AVPacket *queuePacket = av_packet_clone(packet);
            while (d->m_inputQueue.size() >= 50) {
                QThread::msleep(4);
            }
            QMutexLocker lock(&d->m_inputQueueMutex);
            d->m_inputQueue.append(queuePacket);
        }
    }

    void DecoderVAAPI::run() {
        Q_D(AVQt::DecoderVAAPI);

        while (d->m_running) {
            if (!d->m_paused.load() && !d->m_inputQueue.isEmpty()) {
                int ret;
                constexpr size_t strBufSize = 64;
                char strBuf[strBufSize];
                // If m_pCodecParams is nullptr, it is not initialized by packet source, if video codec context is nullptr, this is the first packet
                if (d->m_pCodecParams && !d->m_pCodecCtx) {
                    d->m_pCodec = avcodec_find_decoder(d->m_pCodecParams->codec_id);
                    if (!d->m_pCodec) {
                        qFatal("No video decoder found");
                    }

                    d->m_pCodecCtx = avcodec_alloc_context3(d->m_pCodec);
                    if (!d->m_pCodecCtx) {
                        qFatal("Could not allocate video decoder context, probably out of memory");
                    }

                    ret = av_hwdevice_ctx_create(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
                    if (ret != 0) {
                        qFatal("%d: Could not create AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    avcodec_parameters_to_context(d->m_pCodecCtx, d->m_pCodecParams);
                    d->m_pCodecCtx->hw_device_ctx = av_buffer_ref(d->m_pDeviceCtx);
                    d->m_pCodecCtx->time_base = d->m_timebase;
                    //                    d->m_pCodecCtx->pix_fmt = AV_PIX_FMT_VAAPI;
                    d->m_pCodecCtx->get_format = &DecoderVAAPIPrivate::getFormat;
                    ret = avcodec_open2(d->m_pCodecCtx, d->m_pCodec, nullptr);
                    if (ret != 0) {
                        qFatal("%d: Could not open VAAPI decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    for (const auto &cb : d->m_cbList) {
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
                            qFatal("%d: Error sending packet to VAAPI decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                        }
                        av_packet_free(&packet);
                        lock.relock();
                    }
                }
                while (true) {
                    AVFrame *frame = av_frame_alloc();
                    ret = avcodec_receive_frame(d->m_pCodecCtx, frame);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret == -12) {// -12 == Out of memory, didn't know the macro name
                        av_frame_free(&frame);
                        msleep(1);
                        continue;
                    } else if (ret < 0) {
                        qFatal("%d: Error receiving frame %d from VAAPI decoder: %s", ret, d->m_pCodecCtx->frame_number,
                               av_make_error_string(strBuf, strBufSize, ret));
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
                    for (const auto &cb : d->m_cbList) {
                        //                        cbFutures.append(QtConcurrent::run([=] {
                        AVFrame *cbFrame = av_frame_clone(frame);
                        cbFrame->pts = av_rescale_q(frame->pts, d->m_timebase,
                                                    av_make_q(1, 1000000));// Rescale pts to microseconds for easier processing
                        qDebug("Calling video frame callback for PTS: %lld, Timebase: %d/%d", static_cast<long long>(cbFrame->pts),
                               1,
                               1000000);
                        QTime time = QTime::currentTime();
                        cb->onFrame(this, cbFrame, static_cast<int64_t>(av_q2d(av_inv_q(d->m_framerate)) * 1000.0), d->m_pDeviceCtx);
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
                    av_frame_free(&frame);
                }
                //                av_frame_free(&frame);
            } else {
                msleep(4);
            }
        }
    }

    AVPixelFormat DecoderVAAPIPrivate::getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts) {
        auto *iFmt = pix_fmts;
        AVPixelFormat result = AV_PIX_FMT_NONE;
        while (*iFmt != AV_PIX_FMT_NONE) {
            if (*iFmt == AV_PIX_FMT_VAAPI) {
                result = *iFmt;
                break;
            }
            ++iFmt;
        }

        ctx->hw_frames_ctx = av_hwframe_ctx_alloc(ctx->hw_device_ctx);
        auto framesContext = reinterpret_cast<AVHWFramesContext *>(ctx->hw_frames_ctx->data);
        framesContext->width = ctx->width;
        framesContext->height = ctx->height;
        framesContext->format = result;
        framesContext->sw_format = ctx->sw_pix_fmt;
        framesContext->initial_pool_size = 100;
        int ret = av_hwframe_ctx_init(ctx->hw_frames_ctx);
        if (ret != 0) {
            char strBuf[64];
            qFatal("[AVQt::DecoderVAAPI] %d: Could not initialize HW frames context: %s", ret, av_make_error_string(strBuf, 64, ret));
        }
        return result;
    }
}// namespace AVQt
