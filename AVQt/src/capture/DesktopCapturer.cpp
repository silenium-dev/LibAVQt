//
// Created by silas on 27.01.22.
//

#include "capture/DesktopCapturer.hpp"
#include "capture/DesktopCaptureFactory.hpp"
#include "private/DesktopCapturer_p.hpp"

#include "communication/Message.hpp"

#include "global.hpp"

#include <pgraph_network/impl/RegisteringPadFactory.hpp>

namespace AVQt {
    DesktopCapturer::DesktopCapturer(const api::IDesktopCaptureImpl::Config &config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProducer(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new DesktopCapturerPrivate(this)) {
        Q_D(DesktopCapturer);
        d->config = config;
        d->impl = DesktopCaptureFactory::getInstance().createCapture("AVQt::DesktopCapture");
    }

    DesktopCapturer::~DesktopCapturer() {
        Q_D(DesktopCapturer);
        if (d->open) {
            DesktopCapturer::close();
        }
    }

    bool DesktopCapturer::isOpen() const {
        Q_D(const DesktopCapturer);
        return d->open;
    }

    bool DesktopCapturer::isRunning() const {
        Q_D(const DesktopCapturer);
        return d->running;
    }

    bool DesktopCapturer::isPaused() const {
        Q_D(const DesktopCapturer);
        return d->paused;
    }

    bool DesktopCapturer::init() {
        Q_D(DesktopCapturer);

        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            d->outputPadUserData = std::make_shared<communication::VideoPadParams>();
            d->outputPadId = createOutputPad(d->outputPadUserData);
            return true;
        } else {
            qWarning() << "DesktopCapturer is already initialized";
            return false;
        }
    }

    bool DesktopCapturer::open() {
        Q_D(DesktopCapturer);

        if (!d->initialized) {
            qWarning() << "DesktopCapturer::open() called before init()";
            return false;
        }

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            if (!d->impl->open(d->config)) {
                qWarning() << "IDesktopCaptureImpl::open() failed";
                return false;
            }
            *d->outputPadUserData = d->impl->getVideoParams();
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::INIT)
                            .withPayload("videoParams", QVariant::fromValue(d->impl->getVideoParams()))
                            .build(),
                    d->outputPadId);
            return true;
        } else {
            qWarning() << "DesktopCapturer is already open";
            return false;
        }
    }

    void DesktopCapturer::close() {
        Q_D(DesktopCapturer);

        if (d->running) {
            stop();
        }

        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            d->impl->close();
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::CLEANUP)
                            .build(),
                    d->outputPadId);
        } else {
            qWarning() << "DesktopCapturer is not open";
        }
    }

    bool DesktopCapturer::start() {
        Q_D(DesktopCapturer);

        if (!d->open) {
            qWarning() << "DesktopCapturer::start() called before open()";
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            if (!(d->frameReadyConnection =
                          connect(std::dynamic_pointer_cast<QObject>(d->impl).get(),
                                  SIGNAL(frameReady(std::shared_ptr<AVFrame>)),
                                  d,
                                  SLOT(onFrameCaptured(std::shared_ptr<AVFrame>)),
                                  Qt::QueuedConnection))) {
                qWarning() << "Failed to connect to frameReady signal";
                goto fail;
            }
            if (!d->impl->start()) {
                qWarning() << "IDesktopCaptureImpl::start() failed";
                goto fail;
            }
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::START)
                            .build(),
                    d->outputPadId);
            QThread::start();
            d->afterStopThread = thread();
            d->moveToThread(this);
            return true;
        } else {
            qWarning() << "DesktopCapturer is already running";
        }
    fail:
        stop();
        return false;
    }

    void DesktopCapturer::stop() {
        Q_D(DesktopCapturer);

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            if (d->frameReadyConnection) {
                disconnect(d->frameReadyConnection);
            }
            if (QThread::isRunning()) {
                QThread::quit();
                QThread::wait();
            }
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::STOP)
                            .build(),
                    d->outputPadId);
            emit stopped();
        } else {
            qWarning() << "DesktopCapturer is not running";
        }
    }

    void DesktopCapturer::pause(bool state) {
        Q_D(DesktopCapturer);

        if (!d->running) {
            qWarning() << "DesktopCapturer is not running";
        }

        bool shouldBe = !state;
        if (d->paused.compare_exchange_strong(shouldBe, state)) {
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::PAUSE)
                            .withPayload("state", state)
                            .build(),
                    d->outputPadId);
        } else {
            qDebug() << "DesktopCapturer is already:" << (state ? "paused" : "not paused");
        }
    }

    void DesktopCapturer::run() {
        Q_D(DesktopCapturer);
        emit started();

        exec();
        d->moveToThread(d->afterStopThread);
    }

    void DesktopCapturerPrivate::onFrameCaptured(const std::shared_ptr<AVFrame> &frame) {
        Q_Q(DesktopCapturer);

        if (!paused) {
            if (lastFrameSize.width() != frame->width || lastFrameSize.height() != frame->height) {
                q->produce(communication::Message::builder()
                                   .withAction(communication::Message::Action::RESIZE)
                                   .withPayload("size", QSize(frame->width, frame->height))
                                   .withPayload("lastSize", lastFrameSize)
                                   .build(),
                           outputPadId);
                lastFrameSize = QSize(frame->width, frame->height);
            }
            q->produce(communication::Message::builder().withAction(communication::Message::Action::DATA).withPayload("frame", QVariant::fromValue(frame)).build(), outputPadId);
        }
    }
}// namespace AVQt
