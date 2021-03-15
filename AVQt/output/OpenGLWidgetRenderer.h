//
// Created by silas on 3/14/21.
//

#include "IFrameSink.h"
#include <QtOpenGLWidgets>

#ifndef TRANSCODE_OPENGLWIDGETRENDERER_H
#define TRANSCODE_OPENGLWIDGETRENDERER_H

namespace AVQt {
    class OpenGLWidgetRendererPrivate;

    class OpenGLWidgetRenderer : public QOpenGLWidget, public IFrameSink {
    Q_OBJECT
        Q_INTERFACES(AVQt::IFrameSink)

        Q_DECLARE_PRIVATE(AVQt::OpenGLWidgetRenderer)

    public:
        explicit OpenGLWidgetRenderer(QWidget *parent = nullptr);

    public slots:
        Q_INVOKABLE int init() override;

        Q_INVOKABLE int deinit() override;

        Q_INVOKABLE int start() override;

        Q_INVOKABLE int stop() override;

        Q_INVOKABLE void pause(bool paused) override;

        Q_INVOKABLE void onFrame(QImage frame, AVRational timebase, AVRational framerate) override;

        Q_INVOKABLE void onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) override;

    signals:

        void started() override;

        void stopped() override;

    protected:
        explicit OpenGLWidgetRenderer(OpenGLWidgetRendererPrivate &p);

        /*!
         * \private
         */
        OpenGLWidgetRendererPrivate *d_ptr;
    };
}


#endif //TRANSCODE_OPENGLWIDGETRENDERER_H
