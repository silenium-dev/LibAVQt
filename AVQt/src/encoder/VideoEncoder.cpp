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

//
// Created by silas on 28.12.21.
//

#include "AVQt/encoder/VideoEncoder.hpp"
#include "private/VideoEncoder_p.hpp"

#include "AVQt/encoder/VideoEncoderFactory.hpp"

#include "AVQt/communication/Message.hpp"
#include "AVQt/communication/VideoPadParams.hpp"
#include "global.hpp"

#include <pgraph_network/impl/RegisteringPadFactory.hpp>

namespace AVQt {
    VideoEncoder::VideoEncoder(const Config &config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new VideoEncoderPrivate(this)) {
        Q_D(VideoEncoder);
        d->config = config;
    }

    VideoEncoder::VideoEncoder(const Config &config, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::impl::SimplePadFactory::getInstance()),
          d_ptr(new VideoEncoderPrivate(this)) {
        Q_D(VideoEncoder);
        d->config = config;
    }

    VideoEncoder::~VideoEncoder() {
        Q_D(VideoEncoder);
        if (d->open) {
            VideoEncoder::close();
        }
    }

    int64_t VideoEncoder::getInputPadId() const {
        Q_D(const VideoEncoder);
        return d->inputPadId;
    }
    int64_t VideoEncoder::getOutputPadId() const {
        Q_D(const VideoEncoder);
        return d->outputPadId;
    }

    bool VideoEncoder::isPaused() const {
        Q_D(const VideoEncoder);
        return d->paused;
    }

    bool VideoEncoder::isOpen() const {
        Q_D(const VideoEncoder);
        return d->open;
    }
    bool VideoEncoder::isRunning() const {
        Q_D(const VideoEncoder);
        return d->running;
    }

    bool VideoEncoder::init() {
        Q_D(VideoEncoder);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            auto inPadParams = std::make_shared<communication::VideoPadParams>();
            inPadParams->isHWAccel = d->impl->isHWAccel();
            d->inputPadId = createInputPad(inPadParams);
            if (d->inputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning("VideoEncoder: Failed to create input pad");
                return false;
            }
            d->outputPadParams = std::make_shared<communication::PacketPadParams>();
            d->outputPadId = createOutputPad(d->outputPadParams);
            if (d->outputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning("VideoEncoder: Failed to create output pad");
                return false;
            }
            return true;
        } else {
            qWarning("VideoEncoder: Already initialized");
        }
        return false;
    }

    bool VideoEncoder::open() {
        Q_D(VideoEncoder);

        if (!d->initialized) {
            qWarning("VideoEncoder: Not initialized");
            return false;
        }

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            common::PixelFormat inputFormat =
                    d->inputParams.isHWAccel
                            ? common::PixelFormat{d->inputParams.swPixelFormat, d->inputParams.pixelFormat}
                            : common::PixelFormat{d->inputParams.swPixelFormat, AV_PIX_FMT_NONE};
            d->impl = VideoEncoderFactory::getInstance().create(inputFormat, getCodecId(d->config.codec), d->config.encodeParameters, d->config.encoderPriority);

            if (!d->impl) {
                qFatal("No VideoEncoderImpl found");
            }

            if (!d->impl->open(d->inputParams)) {
                d->impl.reset();
                qWarning("VideoEncoder: Failed to open");
                return false;
            }

            connect(std::dynamic_pointer_cast<QObject>(d->impl).get(), SIGNAL(packetReady(std::shared_ptr<AVPacket>)),
                    this, SLOT(onPacketReady(std::shared_ptr<AVPacket>)), Qt::DirectConnection);

            d->outputPadParams->codec = getCodecId(d->config.codec);
            d->outputPadParams->mediaType = AVMEDIA_TYPE_VIDEO;

            d->outputPadParams->codecParams = d->impl->getCodecParameters();

            pgraph::impl::SimpleProcessor::produce(communication::Message::builder()
                                                           .withAction(communication::Message::Action::INIT)
                                                           .withPayload("packetParams", QVariant::fromValue(*d->outputPadParams))
                                                           .withPayload("encodeParams", QVariant::fromValue(d->config.encodeParameters))
                                                           .build(),
                                                   d->outputPadId);

            return true;
        } else {
            qWarning("VideoEncoder: Already open");
        }
        return false;
    }

    void VideoEncoder::close() {
        Q_D(VideoEncoder);

        if (!d->initialized) {
            qWarning("VideoEncoder: Not initialized");
            return;
        }

        if (d->running) {
            stop();
        }

        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            d->impl->close();
            pgraph::impl::SimpleProcessor::produce(communication::Message::builder()
                                                           .withAction(communication::Message::Action::CLEANUP)
                                                           .build(),
                                                   d->outputPadId);
            d->codecParams.reset();
        } else {
            qWarning("VideoEncoder: Not open");
        }
    }

    bool VideoEncoder::start() {
        Q_D(VideoEncoder);

        if (!d->initialized) {
            qWarning("VideoEncoder: Not initialized");
            return false;
        }

        if (!d->open) {
            qWarning("VideoEncoder: Not open");
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->paused = false;
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::START)
                            .build(),
                    d->outputPadId);
            QThread::start();
            return true;
        } else {
            qWarning("VideoEncoder: Already running");
        }
        return false;
    }

    void VideoEncoder::stop() {
        Q_D(VideoEncoder);

        if (!d->initialized) {
            qWarning("VideoEncoder: Not initialized");
            return;
        }

        if (!d->open) {
            qWarning("VideoEncoder: Not open");
            return;
        }

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = false;
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::STOP)
                            .build(),
                    d->outputPadId);
            QThread::quit();
            QThread::wait();
        } else {
            qWarning("VideoEncoder: Not running");
        }
    }

    void VideoEncoder::pause(bool pause) {
        Q_D(VideoEncoder);

        if (!d->initialized) {
            qWarning("VideoEncoder: Not initialized");
            return;
        }

        if (!d->open) {
            qWarning("VideoEncoder: Not open");
            return;
        }

        bool shouldBe = !pause;
        if (d->paused.compare_exchange_strong(shouldBe, pause)) {
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::PAUSE)
                            .withPayload("state", pause)
                            .build(),
                    d->outputPadId);
        } else {
            qWarning("VideoEncoder: Already %s", pause ? "paused" : "resumed");
        }
    }

    void VideoEncoder::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(VideoEncoder);

        if (!d->initialized) {
            qWarning("VideoEncoder: Not initialized");
            return;
        }

        if (pad == d->inputPadId) {
            if (data->getType() == communication::Message::Type) {
                auto message = std::static_pointer_cast<communication::Message>(data);
                switch (static_cast<AVQt::communication::Message::Action::Enum>(message->getAction())) {
                    case communication::Message::Action::INIT:
                        d->inputParams = message->getPayload("videoParams").value<communication::VideoPadParams>();
                        if (!open()) {
                            qWarning("VideoEncoder: Failed to open");
                        }
                        break;
                    case communication::Message::Action::CLEANUP:
                        close();
                        break;
                    case communication::Message::Action::START:
                        if (!start()) {
                            qWarning("VideoEncoder: Failed to start");
                        }
                        break;
                    case communication::Message::Action::STOP:
                        stop();
                        break;
                    case communication::Message::Action::PAUSE:
                        pause(message->getPayload("state").toBool());
                        break;
                    case communication::Message::Action::DATA:
                        d->enqueueData(message->getPayload("frame").value<std::shared_ptr<AVFrame>>());
                        break;
                    default:
                        qFatal("Unimplemented action %s", message->getAction().name().toLocal8Bit().data());
                }
            }
        } else {
            qWarning("VideoEncoder: Unknown pad %ld", pad);
        }
    }

    void VideoEncoder::run() {
        Q_D(VideoEncoder);

        exec();
    }

    void VideoEncoder::onPacketReady(const std::shared_ptr<AVPacket> &packet) {
        Q_D(VideoEncoder);
        produce(communication::Message::builder().withAction(communication::Message::Action::DATA).withPayload("packet", QVariant::fromValue(packet)).build(), d->outputPadId);
    }

    void VideoEncoderPrivate::enqueueData(const std::shared_ptr<AVFrame> &frame) {
        int ret;
        while ((ret = impl->encode(frame)) == EAGAIN) {
            QThread::msleep(2);
        }
        if (ret != EXIT_SUCCESS) {
            char strBuf[256];
            qWarning("VideoEncoder: Failed to encode frame: %s", av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret)));
        }
    }
}// namespace AVQt
