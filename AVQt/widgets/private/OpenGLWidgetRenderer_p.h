//
// Created by silas on 15.09.21.
//

#ifndef LIBAVQT_OPENGLWIDGETRENDERER__H
#define LIBAVQT_OPENGLWIDGETRENDERER__H

#include "renderers/OpenGLRenderer.h"
#include "widgets/OpenGLWidgetRenderer.h"

namespace AVQt {
    class OpenGLWidgetRendererPrivate : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(OpenGLWidgetRendererPrivate)
    private:
        explicit OpenGLWidgetRendererPrivate(OpenGLWidgetRenderer *q) : q_ptr(q){};
        OpenGLWidgetRenderer *q_ptr;

        OpenGLRenderer *m_renderer{nullptr};

        std::atomic_bool m_quitOnClose{false};

        friend class OpenGLWidgetRenderer;
    };
}// namespace AVQt

#endif//LIBAVQT_OPENGLWIDGETRENDERER__H