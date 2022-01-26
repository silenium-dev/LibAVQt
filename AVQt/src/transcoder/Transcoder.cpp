// Copyright (c) 2022.
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
// Created by silas on 13.01.22.
//

#include "transcoder/Transcoder.hpp"
#include "communication/Message.hpp"
#include "decoder/VideoDecoderFactory.hpp"
#include "encoder/VideoEncoderFactory.hpp"
#include "global.hpp"
#include "private/Transcoder_p.hpp"
#include <QtConcurrent>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>


namespace AVQt {
    Transcoder::Transcoder(const QString &codecType, EncodeParameters params, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent), pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)), d_ptr(new TranscoderPrivate(this)) {
        Q_D(Transcoder);
        d->encodeParameters = params;
        d->decoderImpl.reset(DecoderFactory::getInstance().create(codecType));
        d->encoderImpl.reset(EncoderFactory::getInstance().create(codecType, params));
    }

    Transcoder::~Transcoder() {
        Transcoder::close();
        delete d_ptr;
    }

    bool Transcoder::init() {
        Q_D(Transcoder);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            d->inputPadId = pgraph::impl::SimpleProcessor::createInputPad(pgraph::api::PadUserData::emptyUserData());
            if (d->inputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning() << "Failed to create input pad";
                return false;
            }

            d->frameOutputPadId = pgraph::impl::SimpleProcessor::createOutputPad(d->videoOutputPadParams);
            if (d->frameOutputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning() << "Failed to create frameOutput pad";
                return false;
            }

            d->packetOutputPadId = pgraph::impl::SimpleProcessor::createOutputPad(d->packetOutputPadParams);
            if (d->packetOutputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning() << "Failed to create packetOutput pad";
                return false;
            }

            return true;
        }
        return false;
    }

    bool Transcoder::open() {
        Q_D(Transcoder);

        if (!d->initialized) {
            qWarning() << "Transcoder not initialized";
            return false;
        }

        bool shouldBe = false;
        if (d->opened.compare_exchange_strong(shouldBe, true)) {
            if (!d->decoderImpl->open(d->inputParams->codecParams)) {
                qWarning() << "Failed to open decoder";
                goto fail;
            }
            if (!d->encoderImpl->open(d->decoderImpl->getVideoParams())) {
                qWarning() << "Failed to open encoder";
                goto fail;
            }

            connect(std::dynamic_pointer_cast<QObject>(d->decoderImpl).get(), SIGNAL(frameReady(std::shared_ptr<AVFrame>)),
                    this, SLOT(onFrameReady(std::shared_ptr<AVFrame>)), Qt::DirectConnection);
            connect(std::dynamic_pointer_cast<QObject>(d->encoderImpl).get(), SIGNAL(packetReady(std::shared_ptr<AVPacket>)),
                    this, SLOT(onPacketReady(std::shared_ptr<AVPacket>)), Qt::QueuedConnection);

            d->packetOutputPadParams = std::make_shared<PacketPadParams>();
            d->packetOutputPadParams->codec = Encoder::codecId(d->encodeParameters.codec);
            d->packetOutputPadParams->mediaType = AVMEDIA_TYPE_VIDEO;

            d->packetOutputPadParams->codecParams = d->encoderImpl->getCodecParameters();
            d->videoOutputPadParams = std::make_shared<communication::VideoPadParams>(d->decoderImpl->getVideoParams());

            produce(Message::builder()
                            .withAction(Message::Action::INIT)
                            .withPayload("videoParams", QVariant::fromValue(d->videoOutputPadParams))
                            .build(),
                    d->frameOutputPadId);
            produce(Message::builder()
                            .withAction(Message::Action::INIT)
                            .withPayload("packetParams", QVariant::fromValue(*d->packetOutputPadParams))
                            .withPayload("encodeParams", QVariant::fromValue(d->encodeParameters))
                            .withPayload("codecParams", QVariant::fromValue(d->packetOutputPadParams->codecParams))
                            .build(),
                    d->packetOutputPadId);

            return true;
        } else {
            qWarning() << "Transcoder already opened";
            return false;
        }
    fail:
        d->opened = false;
        return false;
    }

    void Transcoder::close() {
        Q_D(Transcoder);

        if (!d->initialized) {
            qWarning() << "Transcoder not initialized";
            return;
        }

        if (d->running) {
            stop();
        }

        bool shouldBe = true;
        if (d->opened.compare_exchange_strong(shouldBe, false)) {
            d->encoderImpl->close();
            d->decoderImpl->close();

            produce(Message::builder()
                            .withAction(Message::Action::CLEANUP)
                            .build(),
                    d->frameOutputPadId);
            produce(Message::builder()
                            .withAction(Message::Action::CLEANUP)
                            .build(),
                    d->packetOutputPadId);
        } else {
            qWarning() << "Transcoder not opened";
        }
    }

    bool Transcoder::isOpen() const {
        Q_D(const Transcoder);
        return d->opened;
    }

    bool Transcoder::isRunning() const {
        Q_D(const Transcoder);
        return d->running;
    }

    bool Transcoder::start() {
        Q_D(Transcoder);

        if (!d->initialized) {
            qWarning() << "Transcoder not initialized";
            return false;
        }

        if (!d->opened) {
            qWarning() << "Transcoder not opened";
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->paused = false;

            produce(Message::builder()
                            .withAction(Message::Action::START)
                            .build(),
                    d->frameOutputPadId);
            produce(Message::builder()
                            .withAction(Message::Action::START)
                            .build(),
                    d->packetOutputPadId);

            QThread::start();

            qDebug("Transcoder thread: %p; main thread: %p", thread(), qApp->thread());

            emit started();

            return true;
        } else {
            qWarning() << "Transcoder already running";
            return false;
        }
    }

    void Transcoder::stop() {
        Q_D(Transcoder);

        if (!d->initialized) {
            qWarning() << "Transcoder not initialized";
            return;
        }

        if (!d->opened) {
            qWarning() << "Transcoder not opened";
            return;
        }

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = false;

            produce(Message::builder()
                            .withAction(Message::Action::STOP)
                            .build(),
                    d->frameOutputPadId);
            produce(Message::builder()
                            .withAction(Message::Action::STOP)
                            .build(),
                    d->packetOutputPadId);

            QThread::quit();
            QThread::wait();

            {
                QMutexLocker locker(&d->inputQueueMutex);
                while (!d->inputQueue.isEmpty()) {
                    av_packet_free(&d->inputQueue.front());
                    d->inputQueue.dequeue();
                }
            }

            emit stopped();
        } else {
            qWarning() << "Transcoder not running";
        }
    }

    int64_t Transcoder::getInputPadId() const {
        Q_D(const Transcoder);
        return d->inputPadId;
    }

    int64_t Transcoder::getFrameOutputPadId() const {
        Q_D(const Transcoder);
        return d->frameOutputPadId;
    }

    int64_t Transcoder::getPacketOutputPadId() const {
        Q_D(const Transcoder);
        return d->packetOutputPadId;
    }

    void Transcoder::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(Transcoder);
        if (data->getType() == Message::Type) {
            auto message = std::static_pointer_cast<Message>(data);
            switch (static_cast<AVQt::Message::Action::Enum>(message->getAction())) {
                case Message::Action::INIT:
                    d->inputParams = std::make_shared<PacketPadParams>(message->getPayload("packetParams").value<PacketPadParams>());
                    open();
                    break;
                case Message::Action::CLEANUP:
                    close();
                    break;
                case Message::Action::START:
                    start();
                    break;
                case Message::Action::STOP:
                    stop();
                    break;
                case Message::Action::PAUSE:
                    pause(message->getPayload("state").toBool());
                    break;
                case Message::Action::DATA:
                    while (d->inputQueue.size() > 16) {
                        QThread::msleep(2);
                    }
                    {
                        QMutexLocker locker(&d->inputQueueMutex);
                        d->inputQueue.enqueue(av_packet_clone(message->getPayload("packet").value<AVPacket *>()));
                    }
                    break;
                default:
                    qWarning() << "Unknown message action";
                    break;
            }
        }
    }

    void Transcoder::pause(bool state) {
        Q_D(Transcoder);
        bool shouldBe = !state;
        if (d->paused.compare_exchange_strong(shouldBe, state)) {
            produce(Message::builder()
                            .withAction(Message::Action::PAUSE)
                            .withPayload("state", state)
                            .build(),
                    d->frameOutputPadId);
            produce(Message::builder()
                            .withAction(Message::Action::PAUSE)
                            .withPayload("state", state)
                            .build(),
                    d->packetOutputPadId);
            emit paused(state);
        } else {
            qDebug() << "Transcoder already " << (state ? "paused" : "running");
        }
    }

    bool Transcoder::isPaused() const {
        Q_D(const Transcoder);
        return d->paused;
    }

    void Transcoder::run() {
        Q_D(Transcoder);
        if (!d->opened) {
            qWarning() << "Transcoder not opened";
            return;
        }
        int ret;
        char strBuf[256];
        while (d->running) {
            if (d->inputQueue.isEmpty() || d->paused) {
                QThread::msleep(2);
                continue;
            }
            QElapsedTimer timer;
            timer.start();
            AVPacket *packet = nullptr;
            {
                QMutexLocker locker(&d->inputQueueMutex);
                packet = d->inputQueue.dequeue();
            }
            ret = d->decoderImpl->decode(packet);
            if (ret == EAGAIN) {
                QMutexLocker locker(&d->inputQueueMutex);
                d->inputQueue.prepend(packet);
                msleep(1);
            } else if (ret < 0) {
                av_packet_free(&packet);
                qWarning() << "Transcoder error: " << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
                break;
            } else {
                av_packet_free(&packet);
            }

            //            qDebug("Encode timing: %lld", timer.elapsed());

            //            while ((packet = d->impl->nextPacket())) {
            //                produce(Message::builder()
            //                                .withAction(Message::Action::DATA)
            //                                .withPayload("packet", QVariant::fromValue(packet))
            //                                .build(),
            //                        d->packetOutputPadId);
            //                av_packet_free(&packet);
            //            }
        }
    }

    void Transcoder::onFrameReady(const std::shared_ptr<AVFrame> &frame) {
        Q_D(Transcoder);
        if (frame && d->running) {
            produce(Message::builder()
                            .withAction(Message::Action::DATA)
                            .withPayload("frame", QVariant::fromValue(frame.get()))
                            .build(),
                    d->frameOutputPadId);
            //            QtConcurrent::run([d, frame]() {
            int ret;
            QElapsedTimer timer;
            timer.start();
            //            auto preparedFrame = d->encoderImpl->prepareFrame(frame.getFBO());
        encode:
            ret = d->encoderImpl->encode(frame.get());
            //            qDebug("Encode time: %lld ns", timer.nsecsElapsed());
            if (ret == EAGAIN) {
                qDebug() << "Encode EAGAIN";
                usleep(100);
                goto encode;
            }
            qDebug("Complete encode time: %lld ns", timer.nsecsElapsed());
            if (ret != EXIT_SUCCESS) {
                char strBuf[256];
                qWarning() << "Transcoder error: " << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
            }
            //            av_frame_free(&preparedFrame);
            //            });
        } else {
            qWarning() << "Frame is nullptr";
        }
    }

    void Transcoder::onPacketReady(const std::shared_ptr<AVPacket> &packet) {
        Q_D(Transcoder);
        if (packet && d->running) {
            produce(Message::builder()
                            .withAction(Message::Action::DATA)
                            .withPayload("packet", QVariant::fromValue(packet.get()))
                            .build(),
                    d->packetOutputPadId);
        }
    }

    TranscoderPrivate::TranscoderPrivate(Transcoder *q)
        : q_ptr(q) {
    }
}// namespace AVQt
