//
// Created by silas on 15.09.21.
//

#ifndef LIBAVQT_OPENGLWIDGETRENDERER_H
#define LIBAVQT_OPENGLWIDGETRENDERER_H

#include "output/IFrameSink.h"

#include <QOpenGLWidget>

namespace AVQt {
    class OpenGLWidgetRendererPrivate;
    class OpenGLRenderer;

    class OpenGLWidgetRenderer : public QOpenGLWidget {
        Q_OBJECT
        Q_DECLARE_PRIVATE(AVQt::OpenGLWidgetRenderer)
        Q_DISABLE_COPY(OpenGLWidgetRenderer)
    public:
        explicit OpenGLWidgetRenderer(QWidget *parent = nullptr);
        ~OpenGLWidgetRenderer() Q_DECL_OVERRIDE;
        OpenGLRenderer *getFrameSink();

        void setQuitOnClose(bool enabled);
        bool quitOnClose();

    protected:
        void initializeGL() Q_DECL_OVERRIDE;
        void paintGL() Q_DECL_OVERRIDE;
        void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
        //        void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;

    protected:
        OpenGLWidgetRendererPrivate *d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_OPENGLWIDGETRENDERER_H