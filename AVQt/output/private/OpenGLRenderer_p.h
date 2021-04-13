/*!
 * \private
 * \internal
 */
#include <QtCore>
#include <QtOpenGL>
#include "../RenderClock.h"
#include "../OpenGLRenderer.h"


#ifndef LIBAVQT_OPENGLRENDERER_P_H
#define LIBAVQT_OPENGLRENDERER_P_H

/*!
 * \private
 */
struct AVFrame;

namespace AVQt {
    class OpenGLRenderer;

    /*!
     * \private
     * \internal
     */
    class OpenGLRendererPrivate {
        explicit OpenGLRendererPrivate(OpenGLRenderer *q) : q_ptr(q) {};

        GLint project(GLdouble objx, GLdouble objy, GLdouble objz, const GLdouble model[16], const GLdouble[16], const GLint viewport[4],
                      GLdouble *winx, GLdouble *winy, GLdouble *winz);

        inline void transformPoint(GLdouble out[4], const GLdouble m[16], const GLdouble in[4]);


        OpenGLRenderer *q_ptr = nullptr;

        QMutex m_renderQueueMutex;
        QQueue<QPair<AVFrame *, int64_t>> m_renderQueue;

        RenderClock *m_clock = nullptr;
        int64_t m_currentFrameTimeout = 1;
        QTime m_duration;
        QTime m_position;
        std::atomic_bool m_updateRequired = true, m_paused = false, m_running = false, m_firstFrame = true;
        std::atomic<qint64> m_updateTimestamp = 0;
        std::chrono::time_point<std::chrono::high_resolution_clock> m_lastFrame;

        QMutex m_currentFrameMutex;
        AVFrame *m_currentFrame = nullptr;

        QOpenGLVertexArrayObject m_vao;
        QOpenGLBuffer m_vbo, m_ibo;
        QOpenGLShaderProgram *m_program = nullptr;
        QOpenGLTexture *m_yTexture = nullptr, *m_uvTexture = nullptr;

        static constexpr uint PROGRAM_VERTEX_ATTRIBUTE = 0;
        static constexpr uint PROGRAM_TEXCOORD_ATTRIBUTE = 1;

        friend class OpenGLRenderer;
    };
}

#endif //LIBAVQT_OPENGLRENDERER_P_H
