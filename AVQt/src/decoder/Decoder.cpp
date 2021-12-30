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

#include "decoder/Decoder.hpp"
#include "communication/Message.hpp"
#include "communication/PacketPadParams.hpp"
#include "communication/VideoPadParams.hpp"
#include "decoder/DecoderFactory.hpp"
#include "decoder/private/Decoder_p.hpp"
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
        setObjectName(decoderName + "Decoder");
        d->impl = DecoderFactory::getInstance().create(decoderName);
        if (!d->impl) {
            qFatal("Decoder %s not found", qPrintable(decoderName));
        }
    }

    Decoder::~Decoder() {
        Decoder::close();
        delete d_ptr;
    }

    int64_t Decoder::getInputPadId() const {
        Q_D(const Decoder);
        return d->inputPadId;
    }
    int64_t Decoder::getOutputPadId() const {
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

    void Decoder::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(Decoder);
        if (data->getType() == Message::Type) {
            auto message = std::dynamic_pointer_cast<Message>(data);
            switch (static_cast<Message::Action::Enum>(message->getAction())) {
                case Message::Action::INIT: {
                    auto params = message->getPayload("codecParams").value<AVCodecParameters *>();
                    d->pCodecParams = avcodec_parameters_alloc();
                    avcodec_parameters_copy(d->pCodecParams, params);
                    if (!open()) {
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

        if (!d->initialized) {
            qWarning() << "Decoder not initialized";
            return false;
        }

        if (d->pCodecParams == nullptr) {
            qWarning() << "Codec parameters not set";
            return false;
        }

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            d->impl->open(d->pCodecParams);

            *d->outputPadParams = d->impl->getVideoParams();

            produce(Message::builder().withAction(Message::Action::INIT).withPayload("videoParams", QVariant::fromValue(*d->outputPadParams)).build(), d->outputPadId);

            return true;
        } else {
            qWarning() << "Decoder already open";
            return false;
        }
    }

    bool Decoder::init() {
        Q_D(Decoder);

        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            auto inPadParams = std::make_shared<PacketPadParams>();
            inPadParams->mediaType = AVMEDIA_TYPE_VIDEO;
            d->inputPadId = createInputPad(inPadParams);
            if (d->inputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning() << "Failed to create input pad";
                return false;
            }

            d->outputPadParams = std::make_shared<VideoPadParams>();
            d->outputPadId = pgraph::impl::SimpleProcessor::createOutputPad(d->outputPadParams);
            if (d->outputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning() << "Failed to create output pad";
                return false;
            }
            return true;
        } else {
            qWarning() << "Decoder already initialized";
        }

        return false;
    }

    void Decoder::close() {
        Q_D(Decoder);

        if (d->running) {
            stop();
        }

        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            d->impl->close();
            if (d->pCodecParams != nullptr) {
                avcodec_parameters_free(&d->pCodecParams);
            }
            d->running = false;
            d->paused = false;
            d->open = false;
            produce(Message::builder().withAction(Message::Action::CLEANUP).build(), d->outputPadId);
        } else {
            qWarning() << "Decoder not open";
        }
    }

    bool Decoder::start() {
        Q_D(Decoder);

        if (!d->open) {
            qWarning() << "Decoder not open";
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->paused = false;
            produce(Message::builder().withAction(Message::Action::START).build(), d->outputPadId);
            QThread::start();
            started();
            return true;
        }
        qWarning() << "Decoder already running";
        return false;
    }

    void Decoder::stop() {
        Q_D(Decoder);
        if (!d->running) {
            qWarning() << "Decoder not running";
            return;
        }

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = false;
            produce(Message::builder().withAction(Message::Action::STOP).build(), d->outputPadId);
            QThread::quit();
            QThread::wait();
            {
                QMutexLocker locker(&d->inputQueueMutex);
                while (!d->inputQueue.empty()) {
                    auto packet = d->inputQueue.dequeue();
                    av_packet_free(&packet);
                }
            }
            stopped();
            return;
        }
        qWarning() << "Decoder not running";
    }

    void Decoder::pause(bool pause) {
        Q_D(Decoder);
        bool shouldBe = !pause;
        if (d->paused.compare_exchange_strong(shouldBe, pause)) {
            produce(Message::builder().withAction(Message::Action::PAUSE).withPayload("state", pause).build(), d->outputPadId);
            paused(pause);
            qDebug("Changed paused state of decoder to %s", pause ? "true" : "false");
        }
    }

    void Decoder::run() {
        Q_D(Decoder);
        while (d->running) {
            AVFrame *frame;
            msleep(2);
            while ((frame = d->impl->nextFrame()) && d->running) {
                msleep(1);
                qDebug("Decoder produced frame with pts %ld", frame->pts);
                produce(Message::builder().withAction(Message::Action::DATA).withPayload("frame", QVariant::fromValue(frame)).build(), d->outputPadId);
                av_frame_free(&frame);
            }
            QMutexLocker lock(&d->inputQueueMutex);
            if (d->paused || d->inputQueue.isEmpty()) {
                lock.unlock();
                msleep(1);
                continue;
            }
            auto packet = d->inputQueue.dequeue();
            lock.unlock();
            int ret = d->impl->decode(packet);
            if (ret == EAGAIN) {
                lock.relock();
                d->inputQueue.prepend(packet);
                lock.unlock();
            } else if (ret != EXIT_SUCCESS) {
                av_packet_free(&packet);
                char strBuf[256];
                qWarning() << "Decoder error" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
            } else {
                av_packet_free(&packet);
            }
        }
    }

    void DecoderPrivate::enqueueData(AVPacket *&packet) {
        QMutexLocker lock(&inputQueueMutex);
        inputQueue.enqueue(av_packet_clone(packet));
    }
}// namespace AVQt
