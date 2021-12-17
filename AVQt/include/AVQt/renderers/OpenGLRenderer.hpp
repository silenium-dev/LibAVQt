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

#ifndef LIBAVQT_OPENGLRENDERER_HPP
#define LIBAVQT_OPENGLRENDERER_HPP

#include "communication/IComponent.hpp"
#include <pgraph/impl/SimpleProcessor.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

#include <QObject>
#include <QOpenGLFunctions>
#include <static_block.hpp>

namespace AVQt {
    class OpenGLRendererPrivate;
    class OpenGLRenderer : public QObject, public api::IComponent, public pgraph::impl::SimpleProcessor, public QOpenGLFunctions {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IComponent)
        Q_DECLARE_PRIVATE(OpenGLRenderer)
    public:
        explicit OpenGLRenderer(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        ~OpenGLRenderer() override = default;

        bool isOpen() const override;
        bool isRunning() const override;
        bool isPaused() const override;

        void consume(uint32_t pad, std::shared_ptr<pgraph::api::Data> data) override;
        bool open() override;
        void close() override;
        bool init() override;
        bool start() override;
        void stop() override;
        void pause(bool state) override;

        void init(QOpenGLContext *context);
        void paint();
    signals:
        void started() override;
        void stopped() override;
        void paused(bool state) override;

    protected:
        OpenGLRendererPrivate *d_ptr;
    };
}// namespace AVQt

#endif//LIBAVQT_OPENGLRENDERER_HPP
