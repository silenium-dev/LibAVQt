/*!
 * \private
 * \internal
 */
#include "../RenderClock.h"
#include "../OpenGLRenderer.h"

#include <QtCore>
#include <QtOpenGL>

#include <va/va.h>
#include <va/va_x11.h>
#include <va/va_drmcommon.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

extern "C" {
#include <libavutil/hwcontext_vaapi.h>
#include <libavutil/hwcontext.h>
}


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
    class OpenGLRendererPrivate : public QObject {
    public:
        OpenGLRendererPrivate(const OpenGLRendererPrivate &) = delete;

        void operator=(const OpenGLRendererPrivate &) = delete;

    private:
        explicit OpenGLRendererPrivate(OpenGLRenderer *q) : q_ptr(q) {};

        [[maybe_unused]] static GLint
        project(GLdouble objx, GLdouble objy, GLdouble objz, const GLdouble model[16], const GLdouble[16], const GLint viewport[4],
                GLdouble *winx, GLdouble *winy, GLdouble *winz);

        static inline void transformPoint(GLdouble out[4], const GLdouble m[16], const GLdouble in[4]);

        static QTime timeFromMillis(int64_t ts);

        OpenGLRenderer *q_ptr{nullptr};

        QMutex m_onFrameMutex{};
        QMutex m_renderQueueMutex{};
        QQueue<QFuture<AVFrame *>> m_renderQueue{};

        RenderClock *m_clock{nullptr};
        QTime m_duration{};
        QTime m_position{};
        std::atomic_bool m_updateRequired{true}, m_paused{false}, m_running{false}, m_firstFrame{true};
        std::atomic<qint64> m_updateTimestamp{0};
        std::chrono::time_point<std::chrono::high_resolution_clock> m_lastFrame{};

        QMutex m_currentFrameMutex{};
        AVFrame *m_currentFrame{nullptr};

        AVBufferRef *m_pQSVDerivedDeviceContext{nullptr};
        AVBufferRef *m_pQSVDerivedFramesContext{nullptr};

        //OpenGL stuff
        QOpenGLVertexArrayObject m_vao{};
        QOpenGLBuffer m_vbo{}, m_ibo{};
        QOpenGLShaderProgram *m_program{nullptr};
        QOpenGLTexture *m_yTexture{nullptr}, *m_uTexture{nullptr}, *m_vTexture{nullptr};

        static constexpr uint PROGRAM_VERTEX_ATTRIBUTE{0};
        static constexpr uint PROGRAM_TEXCOORD_ATTRIBUTE{1};

        // VAAPI stuff
        VADisplay m_VADisplay{nullptr};
        AVVAAPIDeviceContext *m_pVAContext{nullptr};
        EGLDisplay m_EGLDisplay{nullptr};
        EGLImage m_EGLImages[2]{};
        GLuint m_textures[2]{};

        friend class OpenGLRenderer;
    };
}

#endif //LIBAVQT_OPENGLRENDERER_P_H