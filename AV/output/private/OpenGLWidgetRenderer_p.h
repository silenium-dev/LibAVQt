//
// Created by silas on 3/14/21.
//

#include "../OpenGLWidgetRenderer.h"

#ifndef TRANSCODE_OPENGLWIDGETRENDERER_P_H
#define TRANSCODE_OPENGLWIDGETRENDERER_P_H

namespace AV {
    struct OpenGLWidgetRendererPrivate {
        explicit OpenGLWidgetRendererPrivate(OpenGLWidgetRenderer *q) : q_ptr(q) {}

        OpenGLWidgetRenderer *q_ptr;
    };
}

#endif //TRANSCODE_OPENGLWIDGETRENDERER_P_H
