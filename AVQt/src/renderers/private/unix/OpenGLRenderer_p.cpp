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

#include "AVQt/renderers/private/OpenGLRendererOld_p.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <libdrm/drm_fourcc.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drmcommon.h>

#include <EGL/egl.h>
#include <GL/glu.h>


namespace AVQt {
    void OpenGLRendererPrivate::bindResources() {
        m_program->bind();
        if (m_currentFrame->format == AV_PIX_FMT_VAAPI) {
            auto frameContext = reinterpret_cast<AVHWFramesContext *>(m_currentFrame->hw_frames_ctx->data);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_textures[0]);
            if (frameContext->sw_format == AV_PIX_FMT_NV12 || frameContext->sw_format == AV_PIX_FMT_P010) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, m_textures[1]);
            }
        } else {
            if (!m_yTexture->isBound(0)) {
                m_yTexture->bind(0);
                if (m_uTexture) {
                    m_uTexture->bind(1);
                }
                if (m_vTexture) {
                    m_vTexture->bind(2);
                }
            }
        }

        m_vao.bind();
        m_ibo.bind();
    }// Platform

    void OpenGLRendererPrivate::releaseResources() {
        m_vao.release();
        m_vbo.release();
        m_program->release();
        if (m_currentFrame->format == AV_PIX_FMT_VAAPI) {
            auto frameContext = reinterpret_cast<AVHWFramesContext *>(m_currentFrame->hw_frames_ctx->data);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            if (frameContext->sw_format == AV_PIX_FMT_NV12 || frameContext->sw_format == AV_PIX_FMT_P010) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        } else {
            m_yTexture->release(0);
            if (m_uTexture) {
                m_uTexture->release(1);
            }
            if (m_vTexture) {
                m_vTexture->release(2);
            }
        }
    }// Platform

    void OpenGLRendererPrivate::destroyResources() {
        if (m_currentFrame) {
            av_frame_free(&m_currentFrame);
        }

        if (!m_renderQueue.isEmpty()) {
            QMutexLocker lock(&m_renderQueueMutex);

            for (auto &e : m_renderQueue) {
                e.waitForFinished();
                av_frame_unref(e.result());
            }

            m_renderQueue.clear();
        }
        delete m_program;
        m_program = nullptr;

        if (m_ibo.isCreated()) {
            m_ibo.destroy();
        }
        if (m_vbo.isCreated()) {
            m_vbo.destroy();
        }
        if (m_vao.isCreated()) {
            m_vao.destroy();
        }


        delete m_yTexture;
        delete m_uTexture;
        delete m_vTexture;

        m_yTexture = nullptr;
        m_uTexture = nullptr;
        m_vTexture = nullptr;

        if (m_EGLImages[0]) {
            for (auto &EGLImage : m_EGLImages) {
                if (EGLImage != nullptr) {
                    eglDestroyImage(m_EGLDisplay, EGLImage);
                }
            }
        }
    }// Platform
}// namespace AVQt
