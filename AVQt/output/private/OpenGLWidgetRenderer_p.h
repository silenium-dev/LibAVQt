/*!
 * \private
 */
#include "../OpenGLWidgetRenderer.h"

#ifndef TRANSCODE_OPENGLWIDGETRENDERER_P_H
#define TRANSCODE_OPENGLWIDGETRENDERER_P_H

namespace AVQt {
    /*!
     * \private
     */
    struct OpenGLWidgetRendererPrivate {
        explicit OpenGLWidgetRendererPrivate(OpenGLWidgetRenderer *q) : q_ptr(q) {}

        OpenGLWidgetRenderer *q_ptr;
    };
}

#endif //TRANSCODE_OPENGLWIDGETRENDERER_P_H
