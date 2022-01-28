// Copyright (c) 2021-2022.
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

#include "AVQt/decoder/VideoDecoder.hpp"
#include "AVQt/communication/Message.hpp"
#include "AVQt/communication/PacketPadParams.hpp"
#include "AVQt/communication/VideoPadParams.hpp"
#include "AVQt/decoder/VideoDecoderFactory.hpp"
#include "private/VideoDecoder_p.hpp"
#include "global.hpp"

#include <pgraph/api/Data.hpp>
#include <pgraph_network/api/PadRegistry.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

#include <QApplication>


namespace AVQt {
    VideoDecoder::VideoDecoder(const QString &decoderName, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new VideoDecoderPrivate(this)) {
        Q_D(VideoDecoder);
        setObjectName(decoderName + "VideoDecoder");
        d->impl = std::move(VideoDecoderFactory::getInstance().create(decoderName));
        if (!d->impl) {
            qFatal("VideoDecoder %s not found", qPrintable(decoderName));
        }
        connect(std::dynamic_pointer_cast<QObject>(d->impl).get(), SIGNAL(frameReady(std::shared_ptr<AVFrame>)),
                this, SLOT(onFrameReady(std::shared_ptr<AVFrame>)), Qt::DirectConnection);
    }

    VideoDecoder::~VideoDecoder() {
        VideoDecoder::close();
        delete d_ptr;
    }

    int64_t VideoDecoder::getInputPadId() const {
        Q_D(const VideoDecoder);
        return d->inputPadId;
    }
    int64_t VideoDecoder::getOutputPadId() const {
        Q_D(const VideoDecoder);
        return d->outputPadId;
    }

    bool VideoDecoder::isPaused() const {
        Q_D(const VideoDecoder);
        return d->paused;
    }

    bool VideoDecoder::isOpen() const {
        Q_D(const VideoDecoder);
        return d->open;
    }

    bool VideoDecoder::isRunning() const {
        Q_D(const VideoDecoder);
        return d->running;
    }

    void VideoDecoder::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(VideoDecoder);
        if (data->getType() == communication::Message::Type) {
            auto message = std::dynamic_pointer_cast<communication::Message>(data);
            switch (static_cast<communication::Message::Action::Enum>(message->getAction())) {
                case communication::Message::Action::INIT: {
                    auto packetParams = message->getPayload("packetParams").value<std::shared_ptr<const communication::PacketPadParams>>();
                    d->codecParams = packetParams->codecParams;
                    if (!open()) {
                        qWarning() << "Failed to open decoder";
                        return;
                    }
                    break;
                }
                case communication::Message::Action::CLEANUP:
                    close();
                    break;
                case communication::Message::Action::START:
                    if (!start()) {
                        qWarning() << "Failed to start decoder";
                    }
                    break;
                case communication::Message::Action::STOP:
                    stop();
                    break;
                case communication::Message::Action::PAUSE:
                    pause(message->getPayload("state").toBool());
                    break;
                case communication::Message::Action::DATA: {
                    auto packet = message->getPayload("packet").value<std::shared_ptr<AVPacket>>();
                    d->enqueueData(packet);
                    break;
                }
                default:
                    qFatal("Unimplemented action %s", message->getAction().name().toLocal8Bit().data());
            }
        }
    }

    bool VideoDecoder::init() {
        Q_D(VideoDecoder);

        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            auto inPadParams = std::make_shared<communication::PacketPadParams>();
            inPadParams->mediaType = AVMEDIA_TYPE_VIDEO;
            d->inputPadId = createInputPad(inPadParams);
            if (d->inputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning() << "Failed to create input pad";
                return false;
            }

            d->outputPadParams = std::make_shared<communication::VideoPadParams>();
            d->outputPadId = pgraph::impl::SimpleProcessor::createOutputPad(d->outputPadParams);
            if (d->outputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning() << "Failed to create output pad";
                return false;
            }
            return true;
        } else {
            qWarning() << "VideoDecoder already initialized";
        }

        return false;
    }

    bool VideoDecoder::open() {
        Q_D(VideoDecoder);

        if (!d->initialized) {
            qWarning() << "VideoDecoder not initialized";
            return false;
        }

        if (d->codecParams == nullptr) {
            qWarning() << "Codec parameters not set";
            return false;
        }

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            d->impl->open(d->codecParams);

            *d->outputPadParams = d->impl->getVideoParams();

            produce(communication::Message::builder().withAction(communication::Message::Action::INIT).withPayload("videoParams", QVariant::fromValue(*d->outputPadParams)).build(), d->outputPadId);

            return true;
        } else {
            qWarning() << "VideoDecoder already open";
            return false;
        }
    }

    void VideoDecoder::close() {
        Q_D(VideoDecoder);

        if (d->running) {
            stop();
        }

        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            d->impl->close();
            d->codecParams.reset();
            d->running = false;
            d->paused = false;
            d->open = false;
            produce(communication::Message::builder().withAction(communication::Message::Action::CLEANUP).build(), d->outputPadId);
        } else {
            qWarning() << "VideoDecoder not open";
        }
    }

    bool VideoDecoder::start() {
        Q_D(VideoDecoder);

        if (!d->open) {
            qWarning() << "VideoDecoder not open";
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->paused = false;
            produce(communication::Message::builder().withAction(communication::Message::Action::START).build(), d->outputPadId);
            QThread::start();
            started();
            return true;
        }
        qWarning() << "VideoDecoder already running";
        return false;
    }

    void VideoDecoder::stop() {
        Q_D(VideoDecoder);
        if (!d->running) {
            qWarning() << "VideoDecoder not running";
            return;
        }

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = false;
            produce(communication::Message::builder().withAction(communication::Message::Action::STOP).build(), d->outputPadId);
            QThread::quit();
            QThread::wait();
            {
                QMutexLocker locker(&d->inputQueueMutex);
                d->inputQueue.clear();
            }
            stopped();
            return;
        }
        qWarning() << "VideoDecoder not running";
    }

    void VideoDecoder::pause(bool pause) {
        Q_D(VideoDecoder);
        bool shouldBe = !pause;
        if (d->paused.compare_exchange_strong(shouldBe, pause)) {
            produce(communication::Message::builder().withAction(communication::Message::Action::PAUSE).withPayload("state", pause).build(), d->outputPadId);
            paused(pause);
            qDebug("Changed paused state of decoder to %s", pause ? "true" : "false");
        }
    }

    void VideoDecoder::run() {
        Q_D(VideoDecoder);
        while (d->running) {
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
                msleep(1);
            } else if (ret != EXIT_SUCCESS) {
                char strBuf[256];
                qWarning() << "VideoDecoder error" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
            }
        }
    }

    void VideoDecoder::onFrameReady(const std::shared_ptr<AVFrame> &frame) {
        Q_D(VideoDecoder);
        while (d->paused && d->running) {
            msleep(1);
        }
        if (d->running) {
            produce(communication::Message::builder().withAction(communication::Message::Action::DATA).withPayload("frame", QVariant::fromValue(frame)).build(), d->outputPadId);
        }
    }

    void VideoDecoderPrivate::enqueueData(const std::shared_ptr<AVPacket> &packet) {
        QMutexLocker lock(&inputQueueMutex);
        while (inputQueue.size() >= 20 && running) {
            lock.unlock();
            QThread::msleep(2);
            lock.relock();
        }
        inputQueue.enqueue(packet);
    }
}// namespace AVQt
