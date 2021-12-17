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

//
// Created by silas on 15.12.21.
//

#include "include/AVQt/renderers/OpenGLRenderer.hpp"
#include "private/OpenGLRenderer_p.hpp"

#include "communication/Message.hpp"
#include "renderers/OpenGLRendererFactory.hpp"
#include <pgraph/api/Data.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

namespace AVQt {
    OpenGLRenderer::OpenGLRenderer(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QObject(parent),
          pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new OpenGLRendererPrivate(this)) {
        Q_D(OpenGLRenderer);
    }

    bool OpenGLRenderer::open() {
        return true;
    }

    void OpenGLRenderer::close() {
        // Empty, as there is nothing to do here. Everything is done in the graphics thread
    }

    bool OpenGLRenderer::isOpen() const {
        Q_D(const OpenGLRenderer);
        return d->impl->isInitialized();
    }

    bool OpenGLRenderer::isRunning() const {
        Q_D(const OpenGLRenderer);
        return d->running;
    }

    bool OpenGLRenderer::init() {
        return true;
    }

    void OpenGLRenderer::init(QOpenGLContext *context) {
        Q_D(OpenGLRenderer);
        initializeOpenGLFunctions();
        d->impl->initializeGL(context);
    }

    void OpenGLRenderer::paint() {
        Q_D(OpenGLRenderer);
        d->impl->update();
    }

    bool OpenGLRenderer::start() {
        return true;
    }

    void OpenGLRenderer::stop() {
        // Empty, as there is nothing to do here. Everything is done in the graphics thread
    }

    void OpenGLRenderer::pause(bool state) {
        Q_D(OpenGLRenderer);
        d->impl->pause(state);
    }

    bool OpenGLRenderer::isPaused() const {
        Q_D(const OpenGLRenderer);
        return d->impl->isPaused();
    }

    void OpenGLRenderer::consume(uint32_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(OpenGLRenderer);
        if (data->getType() == Message::Type) {
            auto message = std::dynamic_pointer_cast<Message>(data);
            switch (static_cast<Message::Action::Enum>(message->getAction())) {
                case Message::Action::INIT:
                    start();
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
                    d->impl->pause(message->getPayload("state").toBool());
                    break;
                case Message::Action::DATA: {
                    auto frame = message->getPayload("frame").value<AVFrame *>();
                    if (frame->format == AV_PIX_FMT_VAAPI) {
                        d->impl = OpenGLRendererFactory::getInstance().create("VAAPIOpenGLRenderer");
                    }
                    d->impl->enqueue(frame);
                    break;
                }
                case Message::Action::NONE:
                    break;
            }
        }
    }
}// namespace AVQt
