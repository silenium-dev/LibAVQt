//
// Created by silas on 15.09.21.
//

#include "OpenGLWidgetRenderer.h"
#include "private/OpenGLWidgetRenderer_p.h"
namespace AVQt {
    OpenGLWidgetRenderer::OpenGLWidgetRenderer(QWidget *parent) : QOpenGLWidget(parent), d_ptr(new OpenGLWidgetRendererPrivate(this)) {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->m_renderer = new OpenGLRenderer(this);
    }

    OpenGLWidgetRenderer::~OpenGLWidgetRenderer() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        delete d_ptr;
    }

    OpenGLRenderer *OpenGLWidgetRenderer::getFrameSink() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        return d->m_renderer;
    }

    void OpenGLWidgetRenderer::initializeGL() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        makeCurrent();
        d->m_renderer->initializeGL(context(), context()->surface());
        doneCurrent();
    }
    void OpenGLWidgetRenderer::paintGL() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        QSize size = d->m_renderer->getFrameSize();
        if (size.width() > 0 && size.height() > 0) {
            int display_width = width();
            int display_height = (width() * size.height() + size.width() / 2) / size.width();
            if (display_height > height()) {
                display_width = (height() * size.width() + size.height() / 2) / size.height();
                display_height = height();
            }
            qDebug("Viewport (x:%d, y:%d, w:%d, h:%d)", (width() - display_width) / 2, (height() - display_height) / 2, display_width, display_height);
            glViewport((width() - display_width) / 2, (height() - display_height) / 2, display_width, display_height);
        }
        d->m_renderer->paintGL();
        update();
    }

    void OpenGLWidgetRenderer::setQuitOnClose(bool enabled) {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->m_quitOnClose.store(enabled);
    }

    bool OpenGLWidgetRenderer::quitOnClose() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        return d->m_quitOnClose.load();
    }
    void OpenGLWidgetRenderer::closeEvent(QCloseEvent *event) {
        Q_D(AVQt::OpenGLWidgetRenderer);
        QOpenGLWidget::closeEvent(event);
        if (d->m_quitOnClose.load()) {
            QCoreApplication::quit();
        }
    }
    void OpenGLWidgetRenderer::showEvent(QShowEvent *event) {
        QWidget::showEvent(event);

        if (isWindow()) {
            windowHandle()->requestActivate();
        }
    }

    //    void OpenGLWidgetRenderer::resizeEvent(QResizeEvent *e) {
    //        Q_D(AVQt::OpenGLWidgetRenderer);
    //        d->m_renderer->resized(e->size());
    //    }
}// namespace AVQt