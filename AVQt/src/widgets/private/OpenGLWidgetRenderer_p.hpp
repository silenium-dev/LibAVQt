#ifndef LIBAVQT_OPENGLWIDGETRENDERER__H
#define LIBAVQT_OPENGLWIDGETRENDERER__H

#include "renderers/OpenGLRenderer.hpp"
#include "widgets/OpenGLWidgetRenderer.hpp"

#include <QMutex>

namespace AVQt {
    class OpenGLWidgetRendererPrivate : public QObject, public QOpenGLFunctions {
        Q_OBJECT
        Q_DECLARE_PUBLIC(AVQt::OpenGLWidgetRenderer)
        Q_DISABLE_COPY(OpenGLWidgetRendererPrivate)

    signals:
        void contextWanted();

    protected slots:
        void paintOffscreen();

    private:
        explicit OpenGLWidgetRendererPrivate(OpenGLWidgetRenderer *q) : QOpenGLFunctions(), q_ptr(q){};

        QRect frameRect();
        QRect frameRect(QRect containing);

        OpenGLWidgetRenderer *q_ptr;

        OpenGLRenderer *m_renderer{};
        QOpenGLTextureBlitter *m_blitter{};

        std::atomic_bool m_quitOnClose{false}, m_firstFrame{true};
        QRecursiveMutex m_backBufferMutex{}, m_frontBufferMutex{};
        QOpenGLFramebufferObject *m_frontBuffer{nullptr}, *m_backBuffer{nullptr};
        QTimer *m_renderTimer{nullptr};

        void lockRenderer() {
            m_renderMutex.lock();
        }
        void unlockRenderer() {
            m_renderMutex.unlock();
        }
        QMutex *grabMutex() {
            return &m_grabMutex;
        }
        QWaitCondition *grabCond() {
            return &m_grabCond;
        }
        void prepareExit() {
            m_exiting = true;
            m_grabCond.wakeAll();
        }

        QThread *m_thread;
        QMutex m_renderMutex;
        QMutex m_grabMutex;
        QWaitCondition m_grabCond;
        OpenGLWidgetRenderer *m_widget{nullptr};
        bool m_exiting{false}, m_initialized{false};

        friend class OpenGLWidgetRenderer;
    };
}// namespace AVQt

#endif//LIBAVQT_OPENGLWIDGETRENDERER__H
