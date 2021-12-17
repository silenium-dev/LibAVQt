#include "widgets/OpenGLWidgetRenderer.hpp"
#include "private/OpenGLWidgetRenderer_p.hpp"
#include <QOpenGLFunctions>
#include <QtConcurrent>
namespace AVQt {
    OpenGLWidgetRenderer::OpenGLWidgetRenderer(QWidget *parent) : QOpenGLWidget(parent), d_ptr(new OpenGLWidgetRendererPrivate(this)) {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->m_renderer = new OpenGLRendererOld(this);
        QSurfaceFormat format;
        format.setSwapBehavior(QSurfaceFormat::TripleBuffer);
        format.setSamples(2);
        setFormat(format);
        create();

        connect(this, &QOpenGLWidget::aboutToCompose, this, &OpenGLWidgetRenderer::onAboutToCompose);
        connect(this, &QOpenGLWidget::frameSwapped, this, &OpenGLWidgetRenderer::onFrameSwapped);
        connect(this, &QOpenGLWidget::aboutToResize, this, &OpenGLWidgetRenderer::onAboutToResize);
        connect(this, &QOpenGLWidget::resized, this, &OpenGLWidgetRenderer::onResized);

        d->m_thread = new QThread;
        d->moveToThread(d->m_thread);
        connect(d->m_thread, &QThread::finished, d, &QObject::deleteLater);

        connect(this, &OpenGLWidgetRenderer::renderRequested, d, &OpenGLWidgetRendererPrivate::paintOffscreen);
        connect(d, &OpenGLWidgetRendererPrivate::contextWanted, this, &OpenGLWidgetRenderer::grabContext);

        d->m_thread->start();
    }

    OpenGLWidgetRenderer::~OpenGLWidgetRenderer() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        // Manual deletion is required, because OpenGL context won't be valid anymore in OpenGLWidgetPrivate destructor
        d->prepareExit();
        d->m_thread->quit();
        d->m_thread->wait();
        delete d->m_thread;
        delete d->m_renderer;
        d->m_renderer = nullptr;
        delete d_ptr;
    }

    void OpenGLWidgetRenderer::onAboutToCompose() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->lockRenderer();
    }

    void OpenGLWidgetRenderer::onFrameSwapped() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->unlockRenderer();
    }

    void OpenGLWidgetRenderer::onAboutToResize() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->lockRenderer();
    }

    void OpenGLWidgetRenderer::onResized() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->unlockRenderer();
    }

    void OpenGLWidgetRenderer::grabContext() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->lockRenderer();
        QMutexLocker lock(d->grabMutex());
        context()->moveToThread(d->m_thread);
        d->grabCond()->wakeAll();
        d->unlockRenderer();
    }

    OpenGLRendererOld *OpenGLWidgetRenderer::getFrameSink() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        return d->m_renderer;
    }

    //    void OpenGLWidgetRenderer::initializeGL() {
    //        Q_D(AVQt::OpenGLWidgetRenderer);
    //        makeCurrent();
    //        initializeOpenGLFunctions();

    //        d->m_frontBuffer = new QOpenGLFramebufferObject(size(), QOpenGLFramebufferObject::CombinedDepthStencil, GL_TEXTURE_2D, GL_RGBA);
    //        d->m_backBuffer = new QOpenGLFramebufferObject(size(), QOpenGLFramebufferObject::CombinedDepthStencil, GL_TEXTURE_2D, GL_RGBA);


    //        d->m_renderTimer = new QTimer;
    //        d->m_renderTimer->setSingleShot(false);
    //        d->m_renderTimer->setInterval(static_cast<int>(std::floor(1000.0 / screen()->refreshRate())));
    //        connect(d->m_renderTimer, &QTimer::timeout, d, &OpenGLWidgetRendererPrivate::paintOffscreen);

    //        d->paintOffscreen();
    //        doneCurrent();
    //    }
    //    void OpenGLWidgetRenderer::paintGL() {
    //        Q_D(AVQt::OpenGLWidgetRenderer);
    //
    //        if (!d->m_renderTimer->isActive()) {
    //            d->m_renderTimer->start();
    //        }
    //
    //        {
    //            QMutexLocker fbLock(&d->m_frontBufferMutex);
    //            QOpenGLFramebufferObject::blitFramebuffer(nullptr, d->frameRect(), d->m_frontBuffer, QRect({0, 0}, d->frameRect().size()));
    //        }
    //
    //        update();
    //    }

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
    void OpenGLWidgetRenderer::paintEvent(QPaintEvent *ev) {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->paintOffscreen();
    }

    //    void OpenGLWidgetRenderer::resizeEvent(QResizeEvent *e) {
    //        Q_D(AVQt::OpenGLWidgetRenderer);
    //        QMutexLocker bbLock(&d->m_backBufferMutex);
    //        if (d->m_backBuffer) {
    //            makeCurrent();
    //            d->m_backBuffer->release();
    //            delete d->m_backBuffer;
    //            d->m_backBuffer = new QOpenGLFramebufferObject(d->frameRect().size(), QOpenGLFramebufferObject::NoAttachment, GL_TEXTURE_2D, GL_RGB);
    //            doneCurrent();
    //            d->paintOffscreen();
    //        }
    //        QOpenGLWidget::resizeEvent(e);
    //    }

    void OpenGLWidgetRendererPrivate::paintOffscreen() {
        Q_Q(AVQt::OpenGLWidgetRenderer);

        if (m_exiting) {
            return;
        }

        auto ctx = q->context();
        if (!ctx) {
            return;
        }

        m_grabMutex.lock();
        emit contextWanted();
        m_grabCond.wait(&m_grabMutex);
        QMutexLocker lock(&m_renderMutex);
        m_grabMutex.unlock();

        if (m_exiting) {
            ctx->moveToThread(qGuiApp->thread());
            return;
        }

        Q_ASSERT(ctx->thread() == QThread::currentThread());

        q->makeCurrent();

        if (!m_initialized) {
            m_initialized = true;
            initializeOpenGLFunctions();
            m_renderer->initializeGL(q->context());
        }

        //        if (m_renderer->isUpdateRequired()) {
        qDebug("painting offscreen");
        //            QMutexLocker bbLock(&m_backBufferMutex);
        m_renderer->paintFBO(m_backBuffer);
        //            QMutexLocker fbLock(&m_frontBufferMutex);
        //            std::swap(m_backBuffer, m_frontBuffer);
        //            qDebug("Swapped buffers");
        //            if (m_frontBuffer->size() != m_backBuffer->size()) {
        //                m_backBuffer->release();
        //                delete m_backBuffer;
        //                m_backBuffer = new QOpenGLFramebufferObject(frameRect().size(), QOpenGLFramebufferObject::NoAttachment, GL_TEXTURE_2D, GL_RGB);
        //            }

        q->doneCurrent();
        ctx->moveToThread(qGuiApp->thread());
        //        }
        QMetaObject::invokeMethod(q, "update");
    }
    QRect OpenGLWidgetRendererPrivate::frameRect() {
        Q_Q(AVQt::OpenGLWidgetRenderer);
        return frameRect(q->rect());
    }
    QRect OpenGLWidgetRendererPrivate::frameRect(QRect containing) {
        Q_Q(AVQt::OpenGLWidgetRenderer);
        QSize s = m_renderer->getFrameSize();
        if (s.isValid()) {
            QSize windowSize = containing.size();
            int display_width = windowSize.width();
            int display_height = (windowSize.width() * s.height() + s.width() / 2) / s.width();
            if (display_height > windowSize.height()) {
                display_width = (windowSize.height() * s.width() + s.height() / 2) / s.height();
                display_height = windowSize.height();
            }
            QRect result = {containing.x() + (windowSize.width() - display_width) / 2, containing.y() + (windowSize.height() - display_height) / 2, display_width, display_height};
            qDebug() << "Frame rect:" << result;
            return result;
        } else {
            return {};
        }
    }
}// namespace AVQt
