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

#ifndef LIBAVQT_OPENGLWIDGETRENDERER_H
#define LIBAVQT_OPENGLWIDGETRENDERER_H

#include "output/IFrameSink.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QtCore>

namespace AVQt {
    class OpenGLWidgetRendererPrivate;
    class OpenGLRendererOld;

    class OpenGLWidgetRenderer : public QOpenGLWidget {
        Q_OBJECT
        Q_DECLARE_PRIVATE(AVQt::OpenGLWidgetRenderer)
        Q_DISABLE_COPY(OpenGLWidgetRenderer)
    public:
        explicit OpenGLWidgetRenderer(QWidget *parent = nullptr);
        ~OpenGLWidgetRenderer() Q_DECL_OVERRIDE;
        OpenGLRendererOld *getFrameSink();

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
