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
        /*!
         * \brief Standard public constructor
         * @param parent Pointer to parent QWidget for building GUIs, if nullptr, it will be shown in its own window
         */
        explicit OpenGLWidgetRenderer(QWidget *parent = nullptr);

        ~OpenGLWidgetRenderer();

        bool isPaused() override;

        void run();

    public slots:
        Q_INVOKABLE int init() override;

        Q_INVOKABLE int deinit() override;

        Q_INVOKABLE int start() override;

        Q_INVOKABLE int stop() override;

        Q_INVOKABLE void pause(bool pause) override;

        Q_INVOKABLE void onFrame(QImage frame, AVRational timebase, AVRational framerate, int64_t duration) override;

        Q_INVOKABLE void onFrame(AVFrame *frame, AVRational timebase, AVRational framerate, int64_t duration) override;

    signals:

        void started() override;

        void stopped() override;

        void paused(bool pause) override;

    protected:
        explicit OpenGLWidgetRenderer(OpenGLWidgetRendererPrivate &p);

        /*!
         * \private
         */
        void paintEvent(QPaintEvent *event) override;

        /*!
         * \private
         */
        void resizeEvent(QResizeEvent *event) override;

        /*!
         * \private
         */
        OpenGLWidgetRendererPrivate *d_ptr;
    };
}


#endif //TRANSCODE_OPENGLWIDGETRENDERER_H
