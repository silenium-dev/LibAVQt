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
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BELIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORTOR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OROTHER DEALINGS IN THE SOFTWARE.

#include "decoder/Decoder.hpp"
#include "decoder/private/Decoder_p.hpp"
#include "decoder/DecoderFactory.hpp"
#include "communication/Message.hpp"
#include "communication/PacketPadParams.hpp"
#include "communication/VideoPadParams.hpp"
#include "global.hpp"

#include <pgraph/api/Data.hpp>
#include <pgraph/api/PadUserData.hpp>
#include <pgraph_network/api/PadRegistry.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

#include <QApplication>
//#include <QtConcurrent>

//#ifndef DOXYGEN_SHOULD_SKIP_THIS
//#define NOW() std::chrono::high_resolution_clock::now()
//#define TIME_US(t1, t2) std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()
//#endif


namespace AVQt {
    Decoder::Decoder(const QString &decoderName, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new DecoderPrivate(this)) {
        Q_D(Decoder);
        d->padRegistry = std::move(padRegistry);
        d->impl = DecoderFactory::getInstance().create(decoderName);
        if (!d->impl) {
            qFatal("Decoder %s not found", qPrintable(decoderName));
        }
    }

    Decoder::~Decoder() {
        Decoder::close();
        delete d_ptr;
    }

    uint32_t Decoder::getInputPadId() const {
        Q_D(const Decoder);
        return d->inputPadId;
    }
    uint32_t Decoder::getOutputPadId() const {
        Q_D(const Decoder);
        return d->outputPadId;
    }

    bool Decoder::isPaused() const {
        Q_D(const Decoder);
        return d->paused;
    }

    bool Decoder::isOpen() const {
        Q_D(const Decoder);
        return d->open;
    }
    bool Decoder::isRunning() const {
        Q_D(const Decoder);
        return d->running;
    }

    void Decoder::consume(uint32_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(Decoder);
        if (data->getType() == Message::Type) {
            auto message = std::dynamic_pointer_cast<Message>(data);
            switch (static_cast<Message::Action::Enum>(message->getAction())) {
                case Message::Action::INIT: {
                    auto params = message->getPayload("codecParams").value<AVCodecParameters *>();
                    d->pCodecParams = avcodec_parameters_alloc();
                    avcodec_parameters_copy(d->pCodecParams, params);
                    if(!open()) {
                        qWarning() << "Failed to open decoder";
                        return;
                    }
                    break;
                }
                case Message::Action::CLEANUP:
                    close();
                    break;
                case Message::Action::START:
                    if (!start()) {
                        qWarning() << "Failed to start decoder";
                    }
                    break;
                case Message::Action::STOP:
                    stop();
                    break;
                case Message::Action::PAUSE:
                    pause(message->getPayload("state").toBool());
                    break;
                case Message::Action::DATA: {
                    auto *packet = message->getPayload("packet").value<AVPacket *>();
                    d->enqueueData(packet);
                    break;
                }
                default:
                    qFatal("Unimplemented action %s", message->getAction().name().toLocal8Bit().data());
            }
        }
    }

    bool Decoder::open() {
        Q_D(Decoder);

        if (d->pCodecParams == nullptr) {
            qWarning() << "Codec parameters not set";
            return false;
        }

        if (d->open) {
            qWarning() << "Decoder already open";
            return false;
        }

        auto padParams = std::make_shared<VideoPadParams>();
        padParams->frameSize = QSize(d->pCodecParams->width, d->pCodecParams->height);
        padParams->pixelFormat = static_cast<AVPixelFormat>(d->impl->getOutputFormat());
        d->outputPadId = pgraph::impl::SimpleProcessor::createOutputPad(padParams);

        d->impl->open(d->pCodecParams);

        return true;
    }

    bool Decoder::init() {
        Q_D(Decoder);
        d->inputPadId = createInputPad(pgraph::api::PadUserData::emptyUserData());
        return false;
    }

    void Decoder::close() {
        Q_D(Decoder);
        stop();
        d->impl->close();
        if (d->pCodecParams != nullptr) {
            avcodec_parameters_free(&d->pCodecParams);
            d->pCodecParams = nullptr;
        }
        pgraph::impl::SimpleProcessor::destroyOutputPad(d->outputPadId);
    }

    bool Decoder::start() {
        Q_D(Decoder);
        return false;
    }

    void Decoder::stop() {
        Q_D(Decoder);
    }

    void Decoder::pause(bool pause) {
        Q_D(Decoder);
        bool shouldBe = !pause;
        if (d->paused.compare_exchange_strong(shouldBe, pause)) {
            paused(pause);
            qDebug("Changed paused state of decoder to %s", pause ? "true" : "false");
        }
    }

    void Decoder::run() {
        Q_D(Decoder);
        while (d->running) {
            QMutexLocker lock(&d->inputQueueMutex);
            if (d->paused || d->inputQueue.isEmpty()) {
                lock.unlock();
                msleep(1);
                continue;
            }
            auto packet = d->inputQueue.dequeue();
            lock.unlock();
            d->impl->decode(packet);
            AVFrame *frame;
            while ((frame = d->impl->nextFrame())) {
                produce(Message::builder().withAction(Message::Action::DATA).withPayload("frame", QVariant::fromValue(frame)).build(), d->outputPadId);
                av_frame_free(&frame);
            }
        }
    }

    void DecoderPrivate::enqueueData(AVPacket *&packet) {
        QMutexLocker lock(&inputQueueMutex);
        inputQueue.enqueue(av_packet_clone(packet));
    }

    //
    //    void Decoder::open(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
    //                            AVCodecParameters *aParams, AVCodecParameters *sParams) {
    //        Q_UNUSED(aParams)
    //        Q_UNUSED(sParams)
    //        Q_D(AVQt::Decoder);
    //        Q_UNUSED(source)
    //        if (d->pCodecParams) {
    //            avcodec_parameters_free(&d->pCodecParams);
    //        }
    //        d->pCodecParams = avcodec_parameters_alloc();
    //        avcodec_parameters_copy(d->pCodecParams, vParams);
    //        d->duration = duration;
    //        d->framerate = framerate;
    //        d->timebase = timebase;
    //        {
    //            QMutexLocker lock(&d->cbListMutex);
    //            for (const auto &cb : d->cbList) {
    //                cb->open(this, framerate, duration);
    //            }
    //        }
    //        open();
    //    }
    //
    //    int Decoder::open() {
    //        return 0;
    //    }
    //
    //    void Decoder::deinit(IPacketSource *source) {
    //        Q_UNUSED(source)
    //        deinit();
    //    }
    //
    //    int Decoder::deinit() {
    //        Q_D(AVQt::Decoder);
    //
    //        stop();
    //
    //        {
    //            QMutexLocker lock(&d->cbListMutex);
    //            for (const auto &cb : d->cbList) {
    //                cb->deinit(this);
    //            }
    //        }
    //
    //        if (d->pCodecParams) {
    //            avcodec_parameters_free(&d->pCodecParams);
    //            d->pCodecParams = nullptr;
    //        }
    //        if (d->pCodecCtx) {
    //            avcodec_close(d->pCodecCtx);
    //            avcodec_free_context(&d->pCodecCtx);
    //            d->pCodecCtx = nullptr;
    //            d->pCodec = nullptr;
    //            av_buffer_unref(&d->pDeviceCtx);
    //            d->pDeviceCtx = nullptr;
    //        }
    //
    //        return 0;
    //    }
    //
    //    void Decoder::start(IPacketSource *source) {
    //        Q_UNUSED(source)
    //        start();
    //    }
    //
    //    int Decoder::start() {
    //        Q_D(AVQt::Decoder);
    //
    //        bool notRunning = false;
    //        if (d->running.compare_exchange_strong(notRunning, true)) {
    //            d->paused.store(false);
    //            paused(false);
    //
    //            QThread::start();
    //            return 0;
    //        }
    //        return -1;
    //    }
    //
    //    void Decoder::stop(IPacketSource *source) {
    //        Q_UNUSED(source)
    //        stop();
    //    }
    //
    //    int Decoder::stop() {
    //        Q_D(AVQt::Decoder);
    //
    //        bool shouldBeCurrent = true;
    //        if (d->running.compare_exchange_strong(shouldBeCurrent, false)) {
    //            {
    //                //                QMutexLocker lock(&d->cbListMutex);
    //                for (const auto &cb : d->cbList) {
    //                    cb->stop(this);
    //                }
    //            }
    //            {
    //                QMutexLocker lock(&d->inputQueueMutex);
    //                for (auto &packet : d->inputQueue) {
    //                    av_packet_unref(packet);
    //                    av_packet_free(&packet);
    //                }
    //                d->inputQueue.clear();
    //            }
    //
    //            wait();
    //
    //            return 0;
    //        }
    //
    //        return -1;
    //    }
    //
    //    void Decoder::pause(bool pause) {
    //        Q_D(AVQt::Decoder);
    //        if (d->running.load() != pause) {
    //            d->paused.store(pause);
    //
    //            for (const auto &cb : d->cbList) {
    //                cb->pause(this, pause);
    //            }
    //
    //            paused(pause);
    //        }
    //    }
    //
    //    bool Decoder::isPaused() {
    //        Q_D(AVQt::Decoder);
    //        return d->paused.load();
    //    }
    //
    //    qint64 Decoder::registerCallback(IFrameSink *frameSink) {
    //        Q_D(AVQt::Decoder);
    //
    //        QMutexLocker lock(&d->cbListMutex);
    //        if (!d->cbList.contains(frameSink)) {
    //            d->cbList.append(frameSink);
    //            if (d->running.load() && d->pCodecCtx) {
    //                frameSink->open(this, d->framerate, d->duration);
    //                frameSink->start(this);
    //            }
    //            return d->cbList.indexOf(frameSink);
    //        }
    //        return -1;
    //    }
    //
    //    qint64 Decoder::unregisterCallback(IFrameSink *frameSink) {
    //        Q_D(AVQt::Decoder);
    //        QMutexLocker lock(&d->cbListMutex);
    //        if (d->cbList.contains(frameSink)) {
    //            auto result = d->cbList.indexOf(frameSink);
    //            d->cbList.removeOne(frameSink);
    //            frameSink->stop(this);
    //            frameSink->deinit(this);
    //            return result;
    //        }
    //        return -1;
    //    }
    //
    //    void Decoder::onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) {
    //        Q_UNUSED(source)
    //
    //        Q_D(AVQt::Decoder);
    //
    //        if (packetType == IPacketSource::CB_VIDEO) {
    //            AVPacket *queuePacket = av_packet_clone(packet);
    //            while (d->inputQueue.size() >= 50) {
    //                QThread::msleep(4);
    //            }
    //            QMutexLocker lock(&d->inputQueueMutex);
    //            d->inputQueue.append(queuePacket);
    //        }
    //    }

    //    void Decoder::run() {
    //        Q_D(AVQt::Decoder);
    //
    //        while (d->running) {
    //            if (!d->paused.load() && !d->inputQueue.isEmpty()) {
    //                int ret;
    //                constexpr size_t strBufSize = 64;
    //                char strBuf[strBufSize];
    //                // If m_pCodecParams is nullptr, it is not initialized by packet source, if video codec context is nullptr, this is the first packet
    //                if (d->pCodecParams && !d->pCodecCtx) {
    //                    d->pCodec = avcodec_find_decoder(d->pCodecParams->codec_id);
    //                    if (!d->pCodec) {
    //                        qFatal("No video decoder found");
    //                    }
    //
    //                    d->pCodecCtx = avcodec_alloc_context3(d->pCodec);
    //                    if (!d->pCodecCtx) {
    //                        qFatal("Could not allocate video decoder context, probably out of memory");
    //                    }
    //
    //                    ret = av_hwdevice_ctx_create(&d->pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
    //                    if (ret != 0) {
    //                        qFatal("%d: Could not create AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
    //                    }
    //
    //                    avcodec_parameters_to_context(d->pCodecCtx, d->pCodecParams);
    //                    d->pCodecCtx->hw_device_ctx = av_buffer_ref(d->pDeviceCtx);
    //                    d->pCodecCtx->time_base = d->timebase;
    //                    //                    d->pCodecCtx->pix_fmt = AV_PIX_FMT_VAAPI;
    //                    d->pCodecCtx->get_format = &DecoderPrivate::getFormat;
    //                    ret = avcodec_open2(d->pCodecCtx, d->pCodec, nullptr);
    //                    if (ret != 0) {
    //                        qFatal("%d: Could not open VAAPI decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
    //                    }
    //
    //                    for (const auto &cb : d->cbList) {
    //                        cb->start(this);
    //                    }
    //                    started();
    //                }
    //                {
    //                    QMutexLocker lock(&d->inputQueueMutex);
    //                    while (!d->inputQueue.isEmpty()) {
    //                        AVPacket *packet = d->inputQueue.dequeue();
    //                        lock.unlock();
    //
    //                        qDebug("Video packet queue size: %d", d->inputQueue.size());
    //
    //                        ret = avcodec_send_packet(d->pCodecCtx, packet);
    //                        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
    //                            lock.relock();
    //                            d->inputQueue.prepend(packet);
    //                            break;
    //                        } else if (ret < 0) {
    //                            qFatal("%d: Error sending packet to VAAPI decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
    //                        }
    //                        av_packet_free(&packet);
    //                        lock.relock();
    //                    }
    //                }
    //                while (true) {
    //                    AVFrame *frame = av_frame_alloc();
    //                    ret = avcodec_receive_frame(d->pCodecCtx, frame);
    //                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
    //                        break;
    //                    } else if (ret == -12) {// -12 == Out of memory, didn't know the macro name
    //                        av_frame_free(&frame);
    //                        msleep(1);
    //                        continue;
    //                    } else if (ret < 0) {
    //                        qFatal("%d: Error receiving frame %d from VAAPI decoder: %s", ret, d->pCodecCtx->frame_number,
    //                               av_make_error_string(strBuf, strBufSize, ret));
    //                    }
    //
    //                    //                    auto t1 = NOW();
    //                    //                    AVFrame *swFrame = av_frame_alloc();
    //                    //                    ret = av_hwframe_transfer_data(swFrame, frame, 0);
    //                    //                    if (ret != 0) {
    //                    //                        qFatal("%d: Could not get hw frame: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
    //                    //                    }
    //                    //                    auto t3 = NOW();
    //
    //                    QMutexLocker lock2(&d->cbListMutex);
    //                    //                    QList<QFuture<void>> cbFutures;
    //                    for (const auto &cb : d->cbList) {
    //                        //                        cbFutures.append(QtConcurrent::run([=] {
    //                        AVFrame *cbFrame = av_frame_clone(frame);
    //                        cbFrame->pts = av_rescale_q(frame->pts, d->timebase,
    //                                                    av_make_q(1, 1000000));// Rescale pts to microseconds for easier processing
    //                        qDebug("Calling video frame callback for PTS: %lld, Timebase: %d/%d", static_cast<long long>(cbFrame->pts),
    //                               1,
    //                               1000000);
    //                        QTime time = QTime::currentTime();
    //                        cb->onFrame(this, cbFrame, static_cast<int64_t>(av_q2d(av_inv_q(d->framerate)) * 1000.0), d->pDeviceCtx);
    //                        qDebug() << "Video CB time:" << time.msecsTo(QTime::currentTime());
    //                        av_frame_free(&cbFrame);
    //                        //                        }));
    //                    }
    //                    //                    bool cbBusy = true;
    //                    //                    while (cbBusy) {
    //                    //                        cbBusy = false;
    //                    //                        for (const auto &future: cbFutures) {
    //                    //                            if (future.isRunning()) {
    //                    //                                cbBusy = true;
    //                    //                                break;
    //                    //                            }
    //                    //                        }
    //                    //                        usleep(500);
    //                    //                    }
    //                    //                    auto t2 = NOW();
    //                    //                    qDebug("Decoder frame transfer time: %ld us", TIME_US(t1, t3));
    //
    //                    //                    av_frame_free(&swFrame);
    //                    av_frame_free(&frame);
    //                }
    //                //                av_frame_free(&frame);
    //            } else {
    //                msleep(4);
    //            }
    //        }
    //    }

    AVPixelFormat DecoderPrivate::getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts) {
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
            qFatal("[AVQt::Decoder] %d: Could not initialize HW frames context: %s", ret, av_make_error_string(strBuf, 64, ret));
        }
        return result;
    }
}// namespace AVQt
