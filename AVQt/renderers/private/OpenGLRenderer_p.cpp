#include "OpenGLRenderer_p.h"

namespace AVQt {
    std::atomic_bool OpenGLRendererPrivate::resourcesLoaded{false};
    OpenGLRendererPrivate::OpenGLRendererPrivate(OpenGLRenderer *q) : q_ptr(q) {
    }

    GLint OpenGLRendererPrivate::project(GLdouble objx, GLdouble objy, GLdouble objz, const GLdouble model[16], const GLdouble proj[16],
                                         const GLint viewport[4], GLdouble *winx, GLdouble *winy, GLdouble *winz) {
        GLdouble in[4], out[4];

        in[0] = objx;
        in[1] = objy;
        in[2] = objz;
        in[3] = 1.0;
        transformPoint(out, model, in);
        transformPoint(in, proj, out);

        if (in[3] < 0.0001)
            return GL_FALSE;

        in[0] /= in[3];
        in[1] /= in[3];
        in[2] /= in[3];

        *winx = viewport[0] + (1 + in[0]) * viewport[2] / 2;
        *winy = viewport[1] + (1 + in[1]) * viewport[3] / 2;

        *winz = (1 + in[2]) / 2;
        return GL_TRUE;
    }// Generic

    void OpenGLRendererPrivate::transformPoint(GLdouble *out, const GLdouble *m, const GLdouble *in) {
#define M(row, col) m[(col) *4 + (row)]
        out[0] = M(0, 0) * in[0] + M(0, 1) * in[1] + M(0, 2) * in[2] + M(0, 3) * in[3];
        out[1] = M(1, 0) * in[0] + M(1, 1) * in[1] + M(1, 2) * in[2] + M(1, 3) * in[3];
        out[2] = M(2, 0) * in[0] + M(2, 1) * in[1] + M(2, 2) * in[2] + M(2, 3) * in[3];
        out[3] = M(3, 0) * in[0] + M(3, 1) * in[1] + M(3, 2) * in[2] + M(3, 3) * in[3];
#undef M
    }// Generic

    QTime OpenGLRendererPrivate::timeFromMillis(int64_t ts) {
        int ms = static_cast<int>(ts % 1000);
        int s = static_cast<int>((ts / 1000) % 60);
        int m = static_cast<int>((ts / 1000 / 60) % 60);
        int h = static_cast<int>(ts / 1000 / 60 / 60);
        return {h, m, s, ms};
    }// Generic

    void OpenGLRendererPrivate::updatePixelFormat() {
        m_program->bind();
        switch (m_currentFrame->format) {
            case AV_PIX_FMT_DXVA2_VLD:
            case AV_PIX_FMT_D3D11:
            case AV_PIX_FMT_BGRA:
                m_program->setUniformValue("inputFormat", 0);
                break;
            case AV_PIX_FMT_VAAPI: {
                auto format = reinterpret_cast<AVHWFramesContext *>(m_currentFrame->hw_frames_ctx->data)->sw_format;
                if (format == AV_PIX_FMT_NV12 || format == AV_PIX_FMT_P010) {
                    m_program->setUniformValue("inputFormat", 1);
                } else {
                    m_program->setUniformValue("inputFormat", 0);
                }
                break;
            }
            case AV_PIX_FMT_NV12:
            case AV_PIX_FMT_P010:
                m_program->setUniformValue("inputFormat", 1);
                break;
            case AV_PIX_FMT_YUV420P:
                m_program->setUniformValue("inputFormat", 2);
                break;
            case AV_PIX_FMT_YUV420P10:
                m_program->setUniformValue("inputFormat", 3);
                break;
            default:
                qFatal("Unsupported pixel format");
        }
        m_program->release();
    }// Generic
}// namespace AVQt
