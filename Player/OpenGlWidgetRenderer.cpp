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


#include "OpenGlWidgetRenderer.hpp"
#include "AVQt/communication/Message.hpp"
#include "AVQt/renderers/IOpenGLFrameMapper.hpp"
#include "AVQt/renderers/OpenGLFrameMapperFactory.hpp"
#include "OpenGlWidgetRendererPrivate.hpp"
#include "global.hpp"
#include <QtConcurrent>
#include <pgraph/api/Data.hpp>
#include <pgraph/api/PadUserData.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

size_t OpenGLWidgetRendererPrivate::nextId{0};

OpenGLWidgetRenderer::OpenGLWidgetRenderer(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QWidget *parent)
    : pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
      QOpenGLWidget(parent),
      QOpenGLFunctions(),
      d_ptr(new OpenGLWidgetRendererPrivate(this)) {
    Q_D(OpenGLWidgetRenderer);
    qRegisterMetaType<std::shared_ptr<QOpenGLFramebufferObject>>();
    setAttribute(Qt::WA_QuitOnClose, false);
}

OpenGLWidgetRenderer::~OpenGLWidgetRenderer() {
    Q_D(OpenGLWidgetRenderer);

    if (d->mapper) {
        d->mapper->stop();
        d->mapper.reset();
    }

    makeCurrent();
    d->blitter->destroy();
    delete d->blitter;
    d->blitter = nullptr;
    doneCurrent();

    if (d->inputPadId != pgraph::api::INVALID_PAD_ID) {
        pgraph::impl::SimpleProcessor::destroyInputPad(d->inputPadId);
    }
    delete d_ptr;
}

bool OpenGLWidgetRenderer::init() {
    Q_D(OpenGLWidgetRenderer);
    d->inputPadId = createInputPad(pgraph::api::PadUserData::emptyUserData());
    if (d->inputPadId == pgraph::api::INVALID_PAD_ID) {
        qWarning() << "Failed to create input pad";
        return false;
    }
    return true;
}

void OpenGLWidgetRenderer::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
    Q_D(OpenGLWidgetRenderer);
    if (data->getType() == AVQt::communication::Message::Type) {
        auto message = std::static_pointer_cast<AVQt::communication::Message>(data);
        switch (static_cast<AVQt::communication::Message::Action::Enum>(message->getAction())) {
            case AVQt::communication::Message::Action::START:
                if (!start()) {
                    qWarning() << "Failed to start renderer";
                    break;
                }
                break;
            case AVQt::communication::Message::Action::STOP:
                stop();
                break;
            case AVQt::communication::Message::Action::PAUSE:
                pause(message->getPayloads().value("state").toBool());
                break;
            case AVQt::communication::Message::Action::DATA: {
                if (d->mapper) {
                    auto frame = message->getPayloads().value("frame").value<std::shared_ptr<AVFrame>>();
                    d->mapper->enqueueFrame(frame);
                    update();
                }
                break;
            }
            case AVQt::communication::Message::Action::INIT:
                d->params = message->getPayload("videoParams").value<AVQt::communication::VideoPadParams>();
                open();
                break;
            case AVQt::communication::Message::Action::CLEANUP:
                close();
                break;
            default:
                qWarning() << "Unknown action:" << message->getAction().name();
                break;
        }
    }
}

void OpenGLWidgetRenderer::initializeGL() {
    Q_D(OpenGLWidgetRenderer);
    initializeOpenGLFunctions();
    d->blitter = new QOpenGLTextureBlitter;
    d->blitter->create();

    if (d->mapper) {
        d->mapper->initializeGL(context());
        d->mapper->start();
    }

    qDebug() << "Application thread:" << QCoreApplication::instance()->thread();
    qDebug() << "Renderer thread:" << thread();
    qDebug() << "Current thread:" << QThread::currentThread();
    qDebug() << "OpenGL thread:" << context()->thread();
    update();
}

void OpenGLWidgetRenderer::paintGL() {
    Q_D(OpenGLWidgetRenderer);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    bool updateRequired = false;
    int64_t updateTimestamp;
    if (!d->paused) {
        if (d->renderQueue.size() >= 2) {
            auto timestamp = d->getTimestamp();
            if (timestamp >= d->renderQueue.first().first || !d->currentFrame.second) {
                updateRequired = true;
                updateTimestamp = timestamp;
            }
        }
        if (updateRequired && d->renderQueue.size() >= 2) {
            auto frame = d->renderQueue.dequeue();
            while (!d->renderQueue.isEmpty()) {
                if (d->renderQueue.first().first < frame.first) {
                    qDebug() << "Received frame with pts" << d->renderQueue.first().first << "before current timestamp"
                             << frame.first << "Resetting clock";
                    d->renderTimer.invalidate();
                    updateTimestamp = d->renderQueue.first().first;
                    d->lastPaused = d->renderQueue.first().first;
                    d->currentFrame = {};
                }
                if (frame.first <= updateTimestamp) {
                    if (d->renderQueue.first().first >= updateTimestamp || d->renderQueue.size() == 1) {
                        break;
                    } else {
                        qWarning("Discarding video frame at PTS: %lld < PTS: %ld", static_cast<long long>(frame.first), updateTimestamp);
                    }
                }
                frame = d->renderQueue.dequeue();
            }
            qDebug("Deleting frame with PTS: %lld", static_cast<long long>(frame.first));
            d->currentFrame = frame;
        }
    }
    if (d->blitter && d->currentFrame.second) {
        int display_width = width();
        int display_height = (width() * d->currentFrame.second->height() + d->currentFrame.second->width() / 2) / d->currentFrame.second->width();
        if (display_height > height()) {
            display_width = (height() * d->currentFrame.second->width() + d->currentFrame.second->height() / 2) / d->currentFrame.second->height();
            display_height = height();
        }
        //        glViewport((width() - display_width) / 2, (height() - display_height) / 2, display_width, display_height);
        d->blitter->bind();
        QMatrix4x4 target = QOpenGLTextureBlitter::targetTransform(QRect((width() - display_width) / 2, (height() - display_height) / 2, display_width, display_height),
                                                                   QRect(0, 0, QWidget::width(), QWidget::height()));
        //        qDebug() << "Drawing frame with PTS: " << d->currentFrame.first << "to" << target;
        d->blitter->blit(d->currentFrame.second->texture(), target, QOpenGLTextureBlitter::OriginBottomLeft);
        d->blitter->release();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setFont(QFont("Roboto", 20));
        p.setPen(Qt::white);
        p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, QString("Renderer %1 at %2 ms").arg(d->id).arg(d->currentFrame.first / 1000));
    }
    update();
}

void OpenGLWidgetRenderer::onFrameReady(qint64 pts, const std::shared_ptr<QOpenGLFramebufferObject> &fbo) {
    Q_D(OpenGLWidgetRenderer);
    if (d->running) {
        d->renderQueue.enqueue({pts, fbo});
    }
    update();
}

void OpenGLWidgetRenderer::closeEvent(QCloseEvent *event) {
    Q_D(OpenGLWidgetRenderer);
    d->renderQueue.clear();
    d->currentFrame = {};
    if (d->running) {
        QCoreApplication::quit();
    }
    QWidget::closeEvent(event);
}

bool OpenGLWidgetRenderer::isOpen() const {
    return true;
}

bool OpenGLWidgetRenderer::isRunning() const {
    return isVisible();
}

void OpenGLWidgetRenderer::pause(bool state) {
    Q_D(OpenGLWidgetRenderer);
    bool shouldBe = !state;
    if (d->paused.compare_exchange_strong(shouldBe, state)) {
        qDebug("Paused: %d", state);
        paused(state);
        if (state) {
            d->lastPaused += d->renderTimer.nsecsElapsed() / 1000;
            d->renderTimer.invalidate();
        } else {
            if (d->currentFrame.second) {// Only start when a frame is available
                d->renderTimer.restart();
            }
        }
        update();
    } else {
        qDebug("OpenGLWidgetRenderer::pause(%d) called while already in state %d", state, d->paused.load());
    }
}

bool OpenGLWidgetRenderer::isPaused() const {
    Q_D(const OpenGLWidgetRenderer);
    return d->paused;
}

bool OpenGLWidgetRenderer::open() {
    Q_D(OpenGLWidgetRenderer);

    d->mapper = AVQt::OpenGLFrameMapperFactory::getInstance()
                        .create({d->params.isHWAccel ? d->params.swPixelFormat : d->params.pixelFormat, d->params.isHWAccel ? d->params.pixelFormat : AV_PIX_FMT_NONE}, {"VAAPI"});
    // clang-format off
    // Preserve normalized form of Qt SIGNAL/SLOT macro
    connect(std::dynamic_pointer_cast<QObject>(d->mapper).get(), SIGNAL(frameReady(qint64,std::shared_ptr<QOpenGLFramebufferObject>)),
            this, SLOT(onFrameReady(qint64,std::shared_ptr<QOpenGLFramebufferObject>)));
    // clang-format on

    create();

    return true;
}

void OpenGLWidgetRenderer::close() {
    Q_D(OpenGLWidgetRenderer);
    if (d->running) {
        stop();
    }

    if (d->mapper) {
        d->mapper->stop();
        d->mapper.reset();
    }

    destroy();
}

bool OpenGLWidgetRenderer::start() {
    Q_D(OpenGLWidgetRenderer);
    bool shouldBe = false;
    if (d->running.compare_exchange_strong(shouldBe, true)) {
        d->renderQueue.clear();
        QMetaObject::invokeMethod(this, &QWidget::show, Qt::QueuedConnection);
        //        if (d->mapper) {
        //            // Preserve normalized form of Qt SIGNAL/SLOT macro
        //            // clang-format off
        //            connect(std::dynamic_pointer_cast<QObject>(d->mapper).get(), SIGNAL(frameReady(qint64,std::shared_ptr<QOpenGLFramebufferObject>)),
        //                    this, SLOT(onFrameReady(qint64,std::shared_ptr<QOpenGLFramebufferObject>)));
        //            // clang-format on
        //            d->mapper->start();
        //        }
        return true;
    } else {
        qDebug("OpenGLWidgetRenderer::start() called while already running");
        return false;
    }
}

void OpenGLWidgetRenderer::stop() {
    Q_D(OpenGLWidgetRenderer);
    bool shouldBe = true;
    if (d->running.compare_exchange_strong(shouldBe, false)) {
        QWidget::close();
        d->lastPaused = 0;
        d->renderTimer.invalidate();

        //        if (d->mapper) {
        //            d->mapper->stop();
        //            // clang-format off
        //            // Preserve normalized form of Qt SIGNAL/SLOT macro
        //            disconnect(std::dynamic_pointer_cast<QObject>(d->mapper).get(), SIGNAL(frameReady(qint64,std::shared_ptr<QOpenGLFramebufferObject>)),
        //                    this, SLOT(onFrameReady(qint64,std::shared_ptr<QOpenGLFramebufferObject>)));
        //            // clang-format on
        //        }
        d->renderQueue.clear();
    } else {
        qDebug("OpenGLWidgetRenderer::stop() called while not running");
    }
}

int64_t OpenGLWidgetRendererPrivate::getTimestamp() {
    if (!renderTimer.isValid() && !paused) {
        renderTimer.restart();
    }
    auto now = renderTimer.nsecsElapsed() / 1000;
    if (!currentFrame.second) {
        return 0;
    } else if (paused) {
        return lastPaused;
    } else {
        return lastPaused + now;
    }
}
