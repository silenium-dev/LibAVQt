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

#ifndef LIBAVQT_OPENGLWIDGETRENDERER__H
#define LIBAVQT_OPENGLWIDGETRENDERER__H

#include "AVQt/renderers/OpenGLRendererOld.hpp"
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

        OpenGLRendererOld *m_renderer{};
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
