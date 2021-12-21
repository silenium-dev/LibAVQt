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

//
// Created by silas on 15.12.21.
//

#include "VAAPIOpenGLRenderMapper.hpp"
#include "global.hpp"
#include "private/VAAPIOpenGLRenderMapper_p.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <libdrm/drm_fourcc.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drmcommon.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GL/glu.h>

#include <QtConcurrent>
#include <QtOpenGL>


#define LOOKUP_FUNCTION(type, func)               \
    auto(func) = (type) eglGetProcAddress(#func); \
    if (!(func)) {                                \
        qFatal("eglGetProcAddress(" #func ")");   \
    }

std::string eglErrorString(EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "No error";
        case EGL_NOT_INITIALIZED:
            return "EGL not initialized or failed to initialize";
        case EGL_BAD_ACCESS:
            return "Resource inaccessible";
        case EGL_BAD_ALLOC:
            return "Cannot allocate resources";
        case EGL_BAD_ATTRIBUTE:
            return "Unrecognized attribute or attribute value";
        case EGL_BAD_CONTEXT:
            return "Invalid EGL context";
        case EGL_BAD_CONFIG:
            return "Invalid EGL frame buffer configuration";
        case EGL_BAD_CURRENT_SURFACE:
            return "Current surface is no longer valid";
        case EGL_BAD_DISPLAY:
            return "Invalid EGL display";
        case EGL_BAD_SURFACE:
            return "Invalid surface";
        case EGL_BAD_MATCH:
            return "Inconsistent arguments";
        case EGL_BAD_PARAMETER:
            return "Invalid argument";
        case EGL_BAD_NATIVE_PIXMAP:
            return "Invalid native pixmap";
        case EGL_BAD_NATIVE_WINDOW:
            return "Invalid native window";
        case EGL_CONTEXT_LOST:
            return "Context lost";
        default:
            return "Unknown error " + std::to_string(int(error));
    }
}

namespace AVQt {

    VAAPIOpenGLRenderMapper::VAAPIOpenGLRenderMapper(QObject *parent)
        : QThread(parent),
          d_ptr(new VAAPIOpenGLRenderMapperPrivate(this)) {
        Q_D(VAAPIOpenGLRenderMapper);

        loadResources();
    }

    VAAPIOpenGLRenderMapper::~VAAPIOpenGLRenderMapper() {
        Q_D(VAAPIOpenGLRenderMapper);

        if (d->context) {
            d->context->makeCurrent(d->surface);
            d->destroyResources();
            d->context->doneCurrent();
            delete d->context;
            delete d->surface;
        }
    }

    void VAAPIOpenGLRenderMapper::start() {
        Q_D(VAAPIOpenGLRenderMapper);
        if (isRunning()) {
            return;
        }
        if (d->context->thread() != this) {
            d->context->moveToThread(this);
        }
        QThread::start();
    }

    void VAAPIOpenGLRenderMapper::stop() {
        Q_D(VAAPIOpenGLRenderMapper);
        if (!isRunning()) {
            return;
        }
        QThread::requestInterruption();
        QThread::quit();
        QThread::wait();
        QMutexLocker lock(&d->renderQueueMutex);
        while (!d->renderQueue.empty()) {
            auto frame = d->renderQueue.dequeue().result();
            if (frame) {
                av_frame_free(&frame);
            }
        }
    }

    void VAAPIOpenGLRenderMapper::initializeGL(QOpenGLContext *context) {
        Q_D(VAAPIOpenGLRenderMapper);

        d->surface = new QOffscreenSurface();
        d->surface->setFormat(context->format());
        d->surface->create();

        d->context = new QOpenGLContext;
        d->context->setShareContext(context);
        d->context->setFormat(context->format());
        d->context->create();

        auto currentContext = QOpenGLContext::currentContext();

        d->context->makeCurrent(d->surface);

        initializeOpenGLFunctions();

        initializePlatformAPI();

        QByteArray shaderVersionString;

        if (context->isOpenGLES()) {
            shaderVersionString = "#version 300 es\n";
        } else {
            shaderVersionString = "#version 330 core\n";
        }

        QFile vsh{":/shaders/texture.vsh"};
        QFile fsh{":/shaders/texture.fsh"};
        vsh.open(QIODevice::ReadOnly);
        fsh.open(QIODevice::ReadOnly);
        QByteArray vertexShader = vsh.readAll().prepend(shaderVersionString);
        QByteArray fragmentShader = fsh.readAll().prepend(shaderVersionString);
        vsh.close();
        fsh.close();

        d->program = new QOpenGLShaderProgram();
        d->program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
        d->program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);

        d->program->bindAttributeLocation("vertex", VAAPIOpenGLRenderMapperPrivate::PROGRAM_VERTEX_ATTRIBUTE);
        d->program->bindAttributeLocation("texCoord", VAAPIOpenGLRenderMapperPrivate::PROGRAM_TEXCOORD_ATTRIBUTE);

        if (!d->program->link()) {
            qDebug() << "Shader linkers errors:\n"
                     << d->program->log();
        }

        d->program->bind();
        d->program->setUniformValue("textureY", 0);
        d->program->setUniformValue("textureU", 1);
        d->program->setUniformValue("textureV", 2);
        d->program->release();

        d->context->doneCurrent();
        if (currentContext) {
            currentContext->makeCurrent(currentContext->surface());
        }
    }

    void VAAPIOpenGLRenderMapper::initializePlatformAPI() {
        Q_D(VAAPIOpenGLRenderMapper);
        EGLint visual_attr[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                EGL_NONE};
        d->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (d->eglDisplay == EGL_NO_DISPLAY) {
            qFatal("Could not get EGL display: %s", eglErrorString(eglGetError()).c_str());
        }
        if (!eglInitialize(d->eglDisplay, nullptr, nullptr)) {
            qFatal("eglInitialize");
        }
        if (!eglBindAPI(EGL_OPENGL_API)) {
            qFatal("eglBindAPI");
        }

        EGLConfig cfg;
        EGLint cfg_count;
        if (!eglChooseConfig(d->eglDisplay, visual_attr, &cfg, 1, &cfg_count) || (cfg_count < 1)) {
            qFatal("eglChooseConfig: %s", eglErrorString(eglGetError()).c_str());
        }

        qDebug("EGL Version: %s", eglQueryString(d->eglDisplay, EGL_VERSION));
    }

    void VAAPIOpenGLRenderMapper::initializeInterop() {
        Q_D(VAAPIOpenGLRenderMapper);
        // Frame has 64 pixel alignment, set max height coord to cut off additional pixels
        float maxTexHeight = 1.0f;

        if (d->currentFrame->format == AV_PIX_FMT_VAAPI) {
            VASurfaceID vaSurfaceId = reinterpret_cast<uintptr_t>(d->currentFrame->data[3]);
            VAImage vaImage;
            vaDeriveImage(d->vaDisplay, vaSurfaceId, &vaImage);
            maxTexHeight = static_cast<float>(d->currentFrame->height * 1.0 / (vaImage.height + 2.0));
            vaDestroyImage(d->vaDisplay, vaImage.image_id);
        }

        float vertices[] = {
                1, 1, 0,  // top right
                1, -1, 0, // bottom right
                -1, -1, 0,// bottom left
                -1, 1, 0  // top left
        };

        float vertTexCoords[] = {
                0, 0, maxTexHeight, maxTexHeight,
                0, 1, 1, 0};

        std::vector<float> vertexBufferData(5 * 4);// 8 entries per vertex * 4 vertices

        float *buf = vertexBufferData.data();

        for (int v = 0; v < 4; ++v, buf += 5) {
            buf[0] = vertices[3 * v];
            buf[1] = vertices[3 * v + 1];
            buf[2] = vertices[3 * v + 2];

            buf[3] = vertTexCoords[v];
            buf[4] = vertTexCoords[v + 1];
        }

        d->vbo = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        d->vbo.create();
        d->vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        d->vbo.bind();

        d->vbo.allocate(vertexBufferData.data(), static_cast<int>(vertexBufferData.size() * sizeof(float)));

        d->vao.create();
        d->vao.bind();

        uint indices[] = {
                0, 1, 3,// first tri
                1, 2, 3 // second tri
        };

        d->ibo = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
        d->ibo.create();
        d->ibo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        d->ibo.bind();
        d->ibo.allocate(indices, sizeof(indices));

        int stride = 5 * sizeof(float);

        // layout location 0 - vec3 with coords
        d->program->enableAttributeArray(0);
        d->program->setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);

        // layout location 1 - vec2 with texture coordinates
        d->program->enableAttributeArray(1);
        int texCoordsOffset = 3 * sizeof(float);
        d->program->setAttributeBuffer(1, GL_FLOAT, texCoordsOffset, 2, stride);

        // Release (unbind) all
        d->vbo.release();
        d->vao.release();

        auto *framesContext = reinterpret_cast<AVHWFramesContext *>(d->currentFrame->hw_frames_ctx->data);
        if (framesContext->sw_format == AV_PIX_FMT_NV12 || framesContext->sw_format == AV_PIX_FMT_P010) {
            glGenTextures(2, d->textures);
            for (const auto &texture : d->textures) {
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            d->program->bind();
            d->program->setUniformValue("inputFormat", 1);
            d->program->release();
        } else if (framesContext->sw_format == AV_PIX_FMT_BGRA) {
            glGenTextures(1, &d->textures[0]);
            glBindTexture(GL_TEXTURE_2D, d->textures[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            d->program->bind();
            d->program->setUniformValue("inputFormat", 0);
            d->program->release();
        } else {
            qFatal("Invalid pixel format");
        }
        auto err = glGetError();

        if (err != GL_NO_ERROR) {
            qFatal("Could not map EGL image to OGL texture: %#0.4x, %s", err, gluErrorString(err));
        }
    }

    void VAAPIOpenGLRenderMapper::mapFrame() {
        Q_D(VAAPIOpenGLRenderMapper);
        LOOKUP_FUNCTION(PFNEGLCREATEIMAGEKHRPROC, eglCreateImageKHR)
        LOOKUP_FUNCTION(PFNEGLDESTROYIMAGEKHRPROC, eglDestroyImageKHR)
        LOOKUP_FUNCTION(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC, glEGLImageTargetTexture2DOES)

        auto *framesContext = reinterpret_cast<AVHWFramesContext *>(d->currentFrame->hw_frames_ctx->data);
        if (framesContext->sw_format == AV_PIX_FMT_NV12 || framesContext->sw_format == AV_PIX_FMT_P010) {
            qDebug("YUV VAAPI frame");
            for (int i = 0; i < 2; ++i) {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, d->textures[i]);
                eglDestroyImageKHR(d->eglDisplay, d->eglImages[i]);
            }

            VASurfaceID va_surface = reinterpret_cast<uintptr_t>(d->currentFrame->data[3]);

            //                        VAImage vaImage;
            //                        vaDeriveImage(d->vaDisplay, va_surface, &vaImage);
            //                        VABufferInfo vaBufferInfo;
            //                        memset(&vaBufferInfo, 0, sizeof(VABufferInfo));
            //                        vaBufferInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_VA;
            //                        vaAcquireBufferHandle(d->vaDisplay, vaImage.buf, &vaBufferInfo);

            VADRMPRIMESurfaceDescriptor prime;
            if (vaExportSurfaceHandle(d->vaDisplay, va_surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                      VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS, &prime) !=
                VA_STATUS_SUCCESS) {
                qFatal("[AVQt::VAAPIOpenGLRenderMapper] Could not export VA surface handle");
            }
            vaSyncSurface(d->vaDisplay, va_surface);

            uint32_t formats[2];
            char strBuf[AV_FOURCC_MAX_STRING_SIZE];
            switch (prime.fourcc) {
                //                        switch (vaImage.format.fourcc) {
                case VA_FOURCC_P010:
                    formats[0] = DRM_FORMAT_R16;
                    formats[1] = DRM_FORMAT_GR1616;
                    break;
                case VA_FOURCC_NV12:
                    formats[0] = DRM_FORMAT_R8;
                    formats[1] = DRM_FORMAT_GR88;
                    break;
                default:
                    qFatal("Unsupported pixel format: %s", av_fourcc_make_string(strBuf, prime.fourcc));
                    //                                qFatal("Unsupported pixel format: %s", av_fourcc_make_string(strBuf, vaImage.format.fourcc));
            }

            for (int i = 0; i < 2; ++i) {
                if (prime.layers[i].drm_format != formats[i]) {
                    qFatal("[AVQt::VAAPIOpenGLRenderMapper] Invalid pixel format: %s",
                           av_fourcc_make_string(strBuf, prime.layers[i].drm_format));
                }

#define LAYER i
#define PLANE 0
                const EGLint img_attr[]{
                        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(prime.layers[LAYER].drm_format),
                        EGL_WIDTH, static_cast<EGLint>(prime.width / (i + 1)),
                        EGL_HEIGHT, static_cast<EGLint>(prime.height / (i + 1)),
                        EGL_DMA_BUF_PLANE0_FD_EXT, prime.objects[prime.layers[LAYER].object_index[PLANE]].fd,
                        EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(prime.layers[LAYER].offset[PLANE]),
                        EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(prime.layers[LAYER].pitch[PLANE]),
                        EGL_NONE};

                //                            const EGLint *img_attr = new EGLint[]{
                //                                    EGL_WIDTH, vaImage.width,
                //                                    EGL_HEIGHT, vaImage.height,
                //                                    EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(formats[i]),
                //                                    EGL_DMA_BUF_PLANE0_FD_EXT, static_cast<EGLint>(vaBufferInfo.handle),
                //                                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(vaImage.offsets[i]),
                //                                    EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(vaImage.pitches[i]),
                //                                    EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
                //                                    EGL_NONE
                //                            };
                while (eglGetError() != EGL_SUCCESS)
                    ;
                d->eglImages[i] = eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                                    img_attr);

                auto error = eglGetError();
                if (!d->eglImages[i] || d->eglImages[i] == EGL_NO_IMAGE_KHR || error != EGL_SUCCESS) {
                    qFatal("[AVQt::VAAPIOpenGLRenderMapper] Could not create %s EGLImage: %s", (i ? "UV" : "Y"),
                           eglErrorString(error).c_str());
                }
#undef LAYER
#undef PLANE

                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, d->textures[i]);
                while (glGetError() != GL_NO_ERROR)
                    ;
                glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, d->eglImages[i]);
                glBindTexture(GL_TEXTURE_2D, 0);
                auto err = glGetError();

                if (err != GL_NO_ERROR) {
                    qFatal("Could not map EGL image to OGL texture: %#0.4x, %s", err, gluErrorString(err));
                }

                //                            void *data = new uint16_t[prime.width * prime.height];
                //
                //                            glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_SHORT, data);
                //                            QImage image(reinterpret_cast<uint8_t *>(data), prime.width, prime.height, QImage::Format_Grayscale16);
                //                            image.save("output.bmp");

                //                            exit(0);
            }
            //                        vaReleaseBufferHandle(d->vaDisplay, vaImage.buf);
            //                        vaDestroyImage(d->vaDisplay, vaImage.image_id);
            for (int i = 0; i < (int) prime.num_objects; ++i) {
                ::close(prime.objects[i].fd);
            }
        } else if (framesContext->sw_format == AV_PIX_FMT_BGRA) {
            qDebug("BGRA VAAPI frame");

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, d->textures[0]);
            if (d->eglImages[0]) {
                eglDestroyImageKHR(d->eglDisplay, d->eglImages[0]);
            }

            VASurfaceID va_surface = reinterpret_cast<uintptr_t>(d->currentFrame->data[3]);

            //                        VAImage vaImage;
            //                        vaDeriveImage(d->vaDisplay, va_surface, &vaImage);
            //                        VABufferInfo vaBufferInfo;
            //                        memset(&vaBufferInfo, 0, sizeof(VABufferInfo));
            //                        vaBufferInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_VA;
            //                        vaAcquireBufferHandle(d->vaDisplay, vaImage.buf, &vaBufferInfo);

            VADRMPRIMESurfaceDescriptor prime;
            VAStatus vaErr;
            if ((vaErr = vaExportSurfaceHandle(d->vaDisplay, va_surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                               VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS, &prime)) !=
                VA_STATUS_SUCCESS) {
                qFatal("[AVQt::VAAPIOpenGLRenderMapper] Could not export VA surface handle: %s", vaErrorStr(vaErr));
            }
            vaSyncSurface(d->vaDisplay, va_surface);

            uint32_t format;

            if (prime.fourcc == VA_FOURCC_ARGB) {
                format = DRM_FORMAT_ARGB8888;
            } else {
                qFatal("[AVQt::VAAPIOpenGLRenderMapper] Illegal VASurface format");
            }
            if (prime.layers[0].drm_format != format) {
                qFatal("[AVQt::VAAPIOpenGLRenderMapper] Illegal DRMPRIME layer format");
            }

            const EGLint img_attr[]{
                    EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(prime.layers[0].drm_format),
                    EGL_WIDTH, static_cast<EGLint>(prime.width),
                    EGL_HEIGHT, static_cast<EGLint>(prime.height),
                    EGL_DMA_BUF_PLANE0_FD_EXT, prime.objects[prime.layers[0].object_index[0]].fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(prime.layers[0].offset[0]),
                    EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(prime.layers[0].pitch[0]),
                    EGL_NONE};

            while (eglGetError() != EGL_SUCCESS)
                ;
            d->eglImages[0] = eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                                img_attr);

            auto error = eglGetError();
            if (!d->eglImages[0] || d->eglImages[0] == EGL_NO_IMAGE_KHR || error != EGL_SUCCESS) {
                qFatal("[AVQt::VAAPIOpenGLRenderMapper] Could not create RGB EGLImage: %s", eglErrorString(error).c_str());
            }

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, d->textures[0]);

            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, d->eglImages[0]);

            auto err = glGetError();
            if (err != GL_NO_ERROR) {
                qFatal("Could not map EGL image to OGL texture: %#0.4x, %s", err, gluErrorString(err));
            }
            glBindTexture(GL_TEXTURE_2D, 0);
            ::close(prime.objects[0].fd);
        } else {
            qFatal("Invalid pixel format");
        }
    }

    void VAAPIOpenGLRenderMapper::enqueueFrame(AVFrame *frame) {
        Q_D(VAAPIOpenGLRenderMapper);
        if (frame->hw_frames_ctx) {
            auto *framesContext = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data);
            if (framesContext->format != AV_PIX_FMT_VAAPI) {
                qFatal("[AVQt::VAAPIOpenGLRenderMapper] Invalid frame format");
            }
            if (framesContext->sw_format != AV_PIX_FMT_NV12 && framesContext->sw_format != AV_PIX_FMT_P010 && framesContext->sw_format != AV_PIX_FMT_BGRA) {
                qFatal("[AVQt::VAAPIOpenGLRenderMapper] Invalid frame sw format");
            }
            auto queueFrame = QtConcurrent::run([d](AVFrame *input, AVBufferRef *pDeviceCtx) {
                bool shouldBe = true;
                if (d->firstFrame.compare_exchange_strong(shouldBe, false) && input->format == AV_PIX_FMT_VAAPI) {
                    d->pVAContext = static_cast<AVVAAPIDeviceContext *>(reinterpret_cast<AVHWDeviceContext *>(pDeviceCtx->data)->hwctx);
                    d->vaDisplay = d->pVAContext->display;
                }
                return input;
            },
                                                av_frame_clone(frame), av_buffer_ref(framesContext->device_ref));

            QMutexLocker locker(&d->renderQueueMutex);
            while (d->renderQueue.size() > VAAPIOpenGLRenderMapperPrivate::RENDERQUEUE_MAX_SIZE) {
                locker.unlock();
                msleep(2);
                locker.relock();
            }
            d->renderQueue.enqueue(queueFrame);
        }
        av_frame_free(&frame);
    }
    void VAAPIOpenGLRenderMapper::run() {
        Q_D(VAAPIOpenGLRenderMapper);

        while (!isInterruptionRequested()) {
            QMutexLocker locker(&d->renderQueueMutex);
            if (d->renderQueue.isEmpty()) {
                locker.unlock();
                msleep(2);
                continue;
            }
            auto frame = d->renderQueue.dequeue().result();
            locker.unlock();

            bool firstFrame;

            {
                QMutexLocker locker2(&d->currentFrameMutex);
                firstFrame = !d->currentFrame;
                if (d->currentFrame) {
                    av_frame_free(&d->currentFrame);
                }

                d->currentFrame = frame;
            }

            d->context->makeCurrent(d->surface);
            if (firstFrame) {
                initializeInterop();
                auto format = reinterpret_cast<AVHWFramesContext *>(d->currentFrame->hw_frames_ctx->data)->sw_format;
                d->program->bind();
                if (format == AV_PIX_FMT_NV12 || format == AV_PIX_FMT_P010) {
                    d->program->setUniformValue("inputFormat", 1);
                } else {
                    d->program->setUniformValue("inputFormat", 0);
                }
                d->program->release();
                qDebug("First frame");
            }
            mapFrame();
            qDebug("Mapped frame");

            std::shared_ptr<QOpenGLFramebufferObject> fbo = std::make_shared<QOpenGLFramebufferObject>(d->currentFrame->width, d->currentFrame->height, QOpenGLFramebufferObject::CombinedDepthStencil);
            fbo->bind();
            glViewport(0, 0, d->currentFrame->width, d->currentFrame->height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            d->bindResources();
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
            d->releaseResources();
            QOpenGLFramebufferObject::bindDefault();
            d->context->doneCurrent();
            emit frameReady(d->currentFrame->pts, std::move(fbo));
            qDebug("Frame ready");
        }
    }

    void VAAPIOpenGLRenderMapperPrivate::bindResources() {
        program->bind();
        auto frameContext = reinterpret_cast<AVHWFramesContext *>(currentFrame->hw_frames_ctx->data);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        if (frameContext->sw_format == AV_PIX_FMT_NV12 || frameContext->sw_format == AV_PIX_FMT_P010) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, textures[1]);
        }

        vao.bind();
        ibo.bind();
    }

    void VAAPIOpenGLRenderMapperPrivate::releaseResources() {
        vao.release();
        vbo.release();
        program->release();
        auto frameContext = reinterpret_cast<AVHWFramesContext *>(currentFrame->hw_frames_ctx->data);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (frameContext->sw_format == AV_PIX_FMT_NV12 || frameContext->sw_format == AV_PIX_FMT_P010) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void VAAPIOpenGLRenderMapperPrivate::destroyResources() {
        if (currentFrame) {
            av_frame_free(&currentFrame);
        }

        if (!renderQueue.isEmpty()) {
            QMutexLocker lock(&renderQueueMutex);

            for (auto &e : renderQueue) {
                e.waitForFinished();
                av_frame_unref(e.result());
            }

            renderQueue.clear();
        }
        delete program;
        program = nullptr;

        if (ibo.isCreated()) {
            ibo.destroy();
        }
        if (vbo.isCreated()) {
            vbo.destroy();
        }
        if (vao.isCreated()) {
            vao.destroy();
        }

        if (eglImages[0]) {
            for (auto &EGLImage : eglImages) {
                if (EGLImage != nullptr) {
                    eglDestroyImage(eglDisplay, EGLImage);
                }
            }
        }
    }
}// namespace AVQt
