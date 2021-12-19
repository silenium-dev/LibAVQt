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

/*!
 * \private
 * \internal
 */
#include "renderers/OpenGLRendererOld.hpp"
#include "renderers/RenderClock.hpp"

#include <QtCore>
#include <QtOpenGL>

#ifdef Q_OS_LINUX

#include <va/va.h>
#include <va/va_x11.h>
#include <va/va_drmcommon.h>

#elif defined(Q_OS_WINDOWS)

#include <d3d9.h>
#include <d3d11.h>

#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>

extern "C"
{
#ifdef Q_OS_LINUX
#include <libavutil/hwcontext_vaapi.h>
#elif defined(Q_OS_WINDOWS)
#include <libavutil/hwcontext_dxva2.h>
#include <libavutil/hwcontext_d3d11va.h>
#endif
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
    Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::OpenGLRenderer)

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

#ifdef Q_OS_LINUX
        // VAAPI stuff
        VADisplay m_VADisplay{nullptr};
        AVVAAPIDeviceContext *m_pVAContext{nullptr};
#elif defined(Q_OS_WINDOWS)
        // DXVA2 stuff
        IDirect3DDeviceManager9 *m_pD3DManager{nullptr};
        AVDXVA2DeviceContext *m_pDXVAContext{nullptr};
        IDirect3DSurface9 *m_pSharedSurface{nullptr};
        HANDLE m_hSharedSurface{}, m_hSharedTexture{}, m_hDXDevice{};

        // D3D11VA stuff
        AVD3D11VADeviceContext *m_pD3D11VAContext{nullptr};
        ID3D11Device *m_pD3D11Device{nullptr};
        ID3D11DeviceContext *m_pD3D11DeviceCtx{nullptr};
        ID3D11Texture2D *m_pSharedTexture{nullptr};
        ID3D11Texture2D *m_pInputTexture{nullptr};
        ID3D11VideoDevice *m_pVideoDevice{nullptr};
        ID3D11VideoContext *m_pVideoDeviceCtx{nullptr};
        ID3D11VideoProcessorEnumerator *m_pVideoProcEnum{nullptr};
        ID3D11VideoProcessor *m_pVideoProc{nullptr};
        ID3D11VideoProcessorOutputView *m_pVideoProcOutputView{nullptr};
        ID3D11VideoProcessorInputView *m_pVideoProcInputView{nullptr};
#endif
        EGLDisplay m_EGLDisplay{nullptr};
        EGLImage m_EGLImages[2]{};
        GLuint m_textures[2]{};

        friend class OpenGLRenderer;
    };
}

#endif //LIBAVQT_OPENGLRENDERER_P_H
