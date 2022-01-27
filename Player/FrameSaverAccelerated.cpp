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
// Created by silas on 18.12.21.
//

#include "FrameSaverAccelerated.hpp"
#include "global.hpp"

#include <pgraph_network/api/PadRegistry.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

#include <AVQt/communication/Message.hpp>
#include <pgraph/api/Data.hpp>
#include <pgraph/api/PadUserData.hpp>

extern "C" {
#include <libavutil/frame.h>
}

FrameSaverAccelerated::FrameSaverAccelerated(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry)
    : pgraph::impl::SimpleConsumer(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)) {
    qRegisterMetaType<std::shared_ptr<QOpenGLFramebufferObject>>();
    surface = new QOffscreenSurface;
    QSurfaceFormat format;
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(4, 1);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    surface->setFormat(format);
    surface->create();
    context = new QOpenGLContext;
    context->setFormat(format);
    context->create();
    context->makeCurrent(surface);
    initializeOpenGLFunctions();

    mapper = AVQt::OpenGLFrameMapperFactory::getInstance().create("VAAPIOpenGLRenderMapper");
    mapper->initializeGL(context);

    context->doneCurrent();

    // clang-format off
    // Preserve normalized form of Qt SIGNAL/SLOT macro
    connect(dynamic_cast<QObject *>(mapper.get()), SIGNAL(frameReady(qint64,std::shared_ptr<QOpenGLFramebufferObject>)),
            this, SLOT(onFrameReady(qint64,std::shared_ptr<QOpenGLFramebufferObject>)),
            Qt::QueuedConnection);
    // clang-format on
    mapper->start();
}

void FrameSaverAccelerated::onFrameReady(qint64 pts, const std::shared_ptr<QOpenGLFramebufferObject> &fbo) {
    qDebug("Frame %lld ready", pts);
    if (fbo) {
        if (frameCounter % 60 == 0) {
            //            uint8_t *data;
            {
                QMutexLocker lock(&contextMutex);
                context->makeCurrent(surface);
                fbo->toImage(true).save(QString("frame%1.bmp").arg(frameCounter));
                context->doneCurrent();
            }
            //            QImage(data, fbo->width(), fbo->height(), QImage::Format_RGBA8888).mirrored().save(QString("frame.bmp").arg(frameCounter));
            qDebug("Saved frame %lu", frameCounter.load());
            //            delete[] data;
            qDebug("FBO pointer owners: %ld", fbo.use_count());
            //            frameCounter = 0;
        }
        ++frameCounter;
    }
}

FrameSaverAccelerated::~FrameSaverAccelerated() {
    mapper->stop();
    delete context;
    delete surface;
}

void FrameSaverAccelerated::init() {
    outputPadId = pgraph::impl::SimpleConsumer::createInputPad(pgraph::api::PadUserData::emptyUserData());
    if (outputPadId == pgraph::api::INVALID_PAD_ID) {
        qWarning() << "Failed to create output pad";
    }
}

void FrameSaverAccelerated::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
    if (data->getType() == AVQt::communication::Message::Type) {
        auto message = std::dynamic_pointer_cast<AVQt::communication::Message>(data);
        if (message->getAction() == AVQt::communication::Message::Action::DATA && message->getPayloads().contains("frame")) {
            auto frame = message->getPayloads().value("frame").value<std::shared_ptr<AVFrame>>();
            if (frame) {
                qDebug("Consuming frame");
                mapper->enqueueFrame(frame);
            }
        } else if (message->getAction() == AVQt::communication::Message::Action::START) {
            mapper->start();
        } else if (message->getAction() == AVQt::communication::Message::Action::STOP) {
            mapper->stop();
        }
    }
}
