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

//
// Created by silas on 28.12.21.
//

#include "encoder/Encoder.hpp"
#include "communication/Message.hpp"
#include "communication/VideoPadParams.hpp"
#include "encoder/EncoderFactory.hpp"
#include "global.hpp"
#include "pgraph_network/impl/RegisteringPadFactory.hpp"
#include "private/Encoder_p.hpp"

namespace AVQt {
    Encoder::Encoder(const QString &encoderName, const EncodeParameters &encodeParameters, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new EncoderPrivate(this)) {
        Q_D(Encoder);
        d->impl = EncoderFactory::getInstance().create(encoderName, encodeParameters);
        if (!d->impl) {
            qFatal("Encoder %s not found", qPrintable(encoderName));
        }
    }

    Encoder::~Encoder() {
        Encoder::close();
    }

    int64_t Encoder::getInputPadId() const {
        Q_D(const Encoder);
        return d->inputPadId;
    }
    int64_t Encoder::getOutputPadId() const {
        Q_D(const Encoder);
        return d->outputPadId;
    }

    bool Encoder::isPaused() const {
        Q_D(const Encoder);
        return d->paused;
    }

    bool Encoder::isOpen() const {
        Q_D(const Encoder);
        return d->open;
    }
    bool Encoder::isRunning() const {
        Q_D(const Encoder);
        return d->running;
    }

    bool Encoder::init() {
        Q_D(Encoder);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            auto inPadParams = std::make_shared<VideoPadParams>();
            inPadParams->isHWAccel = d->impl->isHWAccel();
            d->inputPadId = createInputPad(inPadParams);
            if (d->inputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning("Encoder: Failed to create input pad");
                return false;
            }
            d->outputPadParams = std::make_shared<PacketPadParams>();
            d->outputPadId = createOutputPad(d->outputPadParams);
            if (d->outputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning("Encoder: Failed to create output pad");
                return false;
            }
            return true;
        } else {
            qWarning("Encoder: Already initialized");
        }
        return false;
    }

    bool Encoder::open() {
        Q_D(Encoder);

        if (!d->initialized) {
            qWarning("Encoder: Not initialized");
            return false;
        }

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            if (!d->impl->open(d->inputParams)) {
                qWarning("Encoder: Failed to open");
                return false;
            }

            d->outputPadParams->codec = codecId(d->encodeParameters.codec);
            d->outputPadParams->mediaType = AVMEDIA_TYPE_VIDEO;

            d->outputPadParams->codecParams = avcodec_parameters_alloc();
            if (!d->outputPadParams->codecParams) {
                qWarning("Encoder: Failed to allocate codec parameters");
                return false;
            }
            d->outputPadParams->codecParams->codec_id = codecId(d->encodeParameters.codec);
            d->outputPadParams->codecParams->width = d->inputParams.frameSize.width();
            d->outputPadParams->codecParams->height = d->inputParams.frameSize.height();
            d->outputPadParams->codecParams->format = d->inputParams.isHWAccel ? d->inputParams.swPixelFormat : d->inputParams.pixelFormat;
            d->outputPadParams->codecParams->bit_rate = d->encodeParameters.bitrate;
            d->outputPadParams->codecParams->codec_type = AVMEDIA_TYPE_VIDEO;

            AVCodecParameters *codecParams = avcodec_parameters_alloc();
            if (!codecParams) {
                qWarning("Encoder: Failed to allocate codec parameters");
                return false;
            }
            avcodec_parameters_copy(codecParams, d->outputPadParams->codecParams);

            pgraph::impl::SimpleProcessor::produce(Message::builder()
                                                           .withAction(Message::Action::INIT)
                                                           .withPayload("packetParams", QVariant::fromValue(*d->outputPadParams))
                                                           .withPayload("encodeParams", QVariant::fromValue(d->encodeParameters))
                                                           .withPayload("codecParams", QVariant::fromValue(codecParams))
                                                           .build(),
                                                   d->outputPadId);

            avcodec_parameters_free(&codecParams);

            return true;
        } else {
            qWarning("Encoder: Already open");
        }
        return false;
    }

    void Encoder::close() {
        Q_D(Encoder);

        if (!d->initialized) {
            qWarning("Encoder: Not initialized");
            return;
        }

        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            stop();
            d->impl->close();
            pgraph::impl::SimpleProcessor::produce(Message::builder()
                                                           .withAction(Message::Action::CLEANUP)
                                                           .build(),
                                                   d->outputPadId);
        } else {
            qWarning("Encoder: Not open");
        }
    }

    bool Encoder::start() {
        Q_D(Encoder);

        if (!d->initialized) {
            qWarning("Encoder: Not initialized");
            return false;
        }

        if (!d->open) {
            qWarning("Encoder: Not open");
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->paused = false;
            produce(Message::builder()
                            .withAction(Message::Action::START)
                            .build(),
                    d->outputPadId);
            QThread::start();
            return true;
        } else {
            qWarning("Encoder: Already running");
        }
        return false;
    }

    void Encoder::stop() {
        Q_D(Encoder);

        if (!d->initialized) {
            qWarning("Encoder: Not initialized");
            return;
        }

        if (!d->open) {
            qWarning("Encoder: Not open");
            return;
        }

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = false;
            produce(Message::builder()
                            .withAction(Message::Action::STOP)
                            .build(),
                    d->outputPadId);
            QThread::quit();
            QThread::wait();
        } else {
            qWarning("Encoder: Not running");
        }
    }

    void Encoder::pause(bool pause) {
        Q_D(Encoder);

        if (!d->initialized) {
            qWarning("Encoder: Not initialized");
            return;
        }

        if (!d->open) {
            qWarning("Encoder: Not open");
            return;
        }

        bool shouldBe = !pause;
        if (d->paused.compare_exchange_strong(shouldBe, pause)) {
            produce(Message::builder()
                            .withAction(Message::Action::PAUSE)
                            .withPayload("state", pause)
                            .build(),
                    d->outputPadId);
        } else {
            qWarning("Encoder: Already %s", pause ? "paused" : "resumed");
        }
    }

    void Encoder::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(Encoder);

        if (!d->initialized) {
            qWarning("Encoder: Not initialized");
            return;
        }

        if (pad == d->inputPadId) {
            if (data->getType() == Message::Type) {
                auto message = std::static_pointer_cast<Message>(data);
                switch (static_cast<AVQt::Message::Action::Enum>(message->getAction())) {
                    case Message::Action::INIT:
                        d->inputParams = message->getPayload("videoParams").value<VideoPadParams>();
                        if (!open()) {
                            qWarning("Encoder: Failed to open");
                        }
                        break;
                    case Message::Action::CLEANUP:
                        stop();
                        break;
                    case Message::Action::START:
                        if (!start()) {
                            qWarning("Encoder: Failed to start");
                        }
                        break;
                    case Message::Action::STOP:
                        stop();
                        break;
                    case Message::Action::PAUSE:
                        pause(message->getPayload("state").toBool());
                        break;
                    case Message::Action::DATA:
                        d->enqueueData(message->getPayload("frame").value<AVFrame *>());
                        break;
                    default:
                        qFatal("Unimplemented action %s", message->getAction().name().toLocal8Bit().data());
                }
            }
        } else {
            qWarning("Encoder: Unknown pad %ld", pad);
        }
    }

    void Encoder::run() {
        Q_D(Encoder);
        while (d->running) {
            AVPacket *packet;
            while ((packet = d->impl->nextPacket()) && d->running) {
                qDebug("Encoder produced packet with pts %ld", packet->pts);
                produce(Message::builder().withAction(Message::Action::DATA).withPayload("packet", QVariant::fromValue(packet)).build(), d->outputPadId);
                av_packet_free(&packet);
            }
            QMutexLocker lock(&d->inputQueueMutex);
            if (d->paused || d->inputQueue.isEmpty()) {
                lock.unlock();
                msleep(1);
                continue;
            }
            auto frame = d->inputQueue.dequeue();
            lock.unlock();
            int ret = d->impl->encode(frame);
            if (ret == EAGAIN) {
                lock.relock();
                d->inputQueue.prepend(frame);
                lock.unlock();
            } else if (ret != EXIT_SUCCESS) {
                char strBuf[256];
                qWarning() << "Encoder error" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
                av_frame_free(&frame);
            } else {
                av_frame_free(&frame);
            }
        }
    }

    AVCodecID Encoder::codecId(Codec codec) {
        AVCodecID result = AV_CODEC_ID_NONE;
        switch (codec) {
            case Codec::H264:
                result = AV_CODEC_ID_H264;
                break;
            case Codec::HEVC:
                result = AV_CODEC_ID_HEVC;
                break;
            case Codec::VP8:
                result = AV_CODEC_ID_VP8;
                break;
            case Codec::VP9:
                result = AV_CODEC_ID_VP9;
                break;
            case Codec::MPEG2:
                result = AV_CODEC_ID_MPEG2VIDEO;
                break;
        }
        return result;
    }

    void EncoderPrivate::enqueueData(AVFrame *frame) {
        QMutexLocker lock(&inputQueueMutex);
        while (inputQueue.size() > 2) {
            lock.unlock();
            QThread::msleep(1);
            lock.relock();
        }
        inputQueue.enqueue(av_frame_clone(frame));
    }
}// namespace AVQt
