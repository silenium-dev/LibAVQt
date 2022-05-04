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


#ifndef LIBAVQT_OPENGLWIDGETRENDERER_HPP
#define LIBAVQT_OPENGLWIDGETRENDERER_HPP

#include "AVQt/communication/IComponent.hpp"
#include "AVQt/renderers/IOpenGLFrameMapper.hpp"
#include "OpenGlWidgetRendererPrivate.hpp"

#include <QtOpenGL>
#include <QOpenGLWidget>
#include <pgraph/impl/SimpleProcessor.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

class OpenGLWidgetRendererPrivate;
class OpenGLWidgetRenderer : public QOpenGLWidget, protected QOpenGLFunctions, public pgraph::impl::SimpleProcessor, public AVQt::api::IComponent {
    Q_OBJECT
    Q_INTERFACES(AVQt::api::IComponent)
    Q_DECLARE_PRIVATE(OpenGLWidgetRenderer)
public:
    explicit OpenGLWidgetRenderer(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QWidget *parent = nullptr);
    ~OpenGLWidgetRenderer() override;

    bool init() override;
    bool open() override;
    void close() override;
    bool start() override;
    void stop() override;

    bool isOpen() const override;
    bool isRunning() const override;
    void pause(bool state) override;
    bool isPaused() const override;

    void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) override;

signals:
    void started() override;
    void stopped() override;
    void paused(bool state) override;

protected:
    void initializeGL() override;
    void paintGL() override;

    void closeEvent(QCloseEvent *event) override;


private slots:
    void onFrameReady(qint64 pts, const std::shared_ptr<QOpenGLFramebufferObject> &fbo);

private:
    OpenGLWidgetRendererPrivate *d_ptr;
};


#endif//LIBAVQT_OPENGLWIDGETRENDERER_HPP
