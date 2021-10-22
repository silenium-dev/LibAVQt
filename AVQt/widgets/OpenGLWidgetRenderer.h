#ifndef LIBAVQT_OPENGLWIDGETRENDERER_H
#define LIBAVQT_OPENGLWIDGETRENDERER_H

#include "output/IFrameSink.h"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QtCore>

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
        //        void initializeGL() Q_DECL_OVERRIDE;
        //        void paintGL() Q_DECL_OVERRIDE;
        void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
        //        void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;

        OpenGLWidgetRendererPrivate *d_ptr;

        void paintEvent(QPaintEvent *ev) Q_DECL_OVERRIDE;
        void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
        //        void resizeEvent(QResizeEvent *e);

    signals:
        void renderRequested();

    protected slots:
        void onAboutToCompose();
        void onFrameSwapped();
        void onAboutToResize();
        void onResized();
        void grabContext();
    };
}// namespace AVQt


#endif//LIBAVQT_OPENGLWIDGETRENDERER_H
