// Copyright (c) 2022.
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
// Created by silas on 23.01.22.
//

#include "DRM_OpenGL_RenderMapper.hpp"
#include "global.hpp"
#include "private/DRM_OpenGL_RenderMapper_p.hpp"

#include <QtConcurrent>
#include <QtCore>
#include <QtGui>
#include <QtOpenGL>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glu.h>
#include <libdrm/drm_fourcc.h>


namespace AVQt {
    DRM_OpenGL_RenderMapper::DRM_OpenGL_RenderMapper(QObject *parent)
        : QThread(parent), d_ptr(new DRM_OpenGL_RenderMapperPrivate(this)) {
        loadResources();
        Q_D(DRM_OpenGL_RenderMapper);
    }

    DRM_OpenGL_RenderMapper::~DRM_OpenGL_RenderMapper() = default;

    void DRM_OpenGL_RenderMapper::initializeGL(QOpenGLContext *shareContext) {
        Q_D(DRM_OpenGL_RenderMapper);

        d->surface = std::make_unique<QOffscreenSurface>();
        d->surface->setFormat(shareContext->format());
        d->surface->create();

        d->context = std::make_unique<QOpenGLContext>();
        d->context->setShareContext(shareContext);
        d->context->setFormat(shareContext->format());
        d->context->create();

        auto currentContext = QOpenGLContext::currentContext();

        d->context->makeCurrent(d->surface.get());

        initializeOpenGLFunctions();

        initializePlatformAPI();

        QByteArray shaderVersionString;

        if (d->context->isOpenGLES()) {
            shaderVersionString = "#version 300 es\n";
        } else {
            shaderVersionString = "#version 330 core\n";
        }

        QFile vsh(":/shaders/textures.vsh");
        QFile fsh(":/shaders/textures.fsh");
        vsh.open(QIODevice::ReadOnly);
        fsh.open(QIODevice::ReadOnly);
        QByteArray vshSource = vsh.readAll().prepend(shaderVersionString);
        QByteArray fshSource = fsh.readAll().prepend(shaderVersionString);
        vsh.close();
        fsh.close();

        d->program = std::make_unique<QOpenGLShaderProgram>();
        d->program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vshSource);
        d->program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fshSource);

        d->program->bindAttributeLocation("vertex", DRM_OpenGL_RenderMapperPrivate::PROGRAM_VERTEX_ATTRIBUTE);
        d->program->bindAttributeLocation("texCoord", DRM_OpenGL_RenderMapperPrivate::PROGRAM_TEXCOORD_ATTRIBUTE);

        if (!d->program->link()) {
            qWarning() << "Failed to link shader program:" << d->program->log();
            d->program.reset();
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

    void DRM_OpenGL_RenderMapper::initializePlatformAPI() {
        Q_D(DRM_OpenGL_RenderMapper);
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
            qFatal("Could not getFBO EGL display: %s", eglErrorString(eglGetError()).c_str());
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

    void DRM_OpenGL_RenderMapper::initializeInterop() {
        Q_D(DRM_OpenGL_RenderMapper);

        float vertices[] = {
                1, 1, 0,  // top right
                1, -1, 0, // bottom right
                -1, -1, 0,// bottom left
                -1, 1, 0  // top left
        };

        float verTexCoords[] = {
                0, 0,
                1, 1,
                0, 1,
                1, 0};

        std::vector<float> vertexBufferData(5 * 4);// 4 vertices * 5 components per vertex
        float *buf = vertexBufferData.data();

        for (int v = 0; v < 4; ++v, buf += 5) {
            buf[0] = vertices[3 * v];
            buf[1] = vertices[3 * v + 1];
            buf[2] = vertices[3 * v + 2];

            buf[3] = verTexCoords[v];
            buf[4] = verTexCoords[v + 1];
        }

        d->vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        d->vbo->create();
        d->vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
        d->vbo->bind();

        d->vbo->allocate(vertexBufferData.data(), static_cast<int>(vertexBufferData.size() * sizeof(float)));

        d->vao = std::make_unique<QOpenGLVertexArrayObject>();
        d->vao->create();
        d->vao->bind();

        uint indices[] = {
                0, 1, 3,// first triangle
                1, 2, 3 // second triangle
        };

        d->ibo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
        d->ibo->create();
        d->ibo->setUsagePattern(QOpenGLBuffer::StaticDraw);
        d->ibo->bind();
        d->ibo->allocate(indices, sizeof(indices));

        int stride = 5 * sizeof(float);

        // layout (location = 0) in vec3 vertex;
        d->program->enableAttributeArray(0);
        d->program->setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);

        // layout (location = 1) in vec2 texCoord;
        d->program->enableAttributeArray(1);
        int texCoordOffset = 3 * sizeof(float);
        d->program->setAttributeBuffer(1, GL_FLOAT, texCoordOffset, 2, stride);

        d->vbo->release();
        d->vao->release();

        auto *frameDescriptor = reinterpret_cast<AVDRMFrameDescriptor *>(d->currentFrame->data[0]);
        if (frameDescriptor->nb_layers != 1) {
            qWarning("DRM_OpenGL_RenderMapper: only support one layer");
            return;
        }
        if (frameDescriptor->layers[0].format == DRM_FORMAT_ARGB8888) {
            glGenTextures(1, d->textures);
            glBindTexture(GL_TEXTURE_2D, d->textures[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            d->program->bind();
            d->program->setUniformValue("inputFormat", 0);
            d->program->release();
        } else if (frameDescriptor->layers[0].format == DRM_FORMAT_NV12 || frameDescriptor->layers[0].format == DRM_FORMAT_P010) {
            glGenTextures(2, d->textures);
            for (unsigned int texture : d->textures) {
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
            d->program->bind();
            d->program->setUniformValue("inputFormat", 1);
            d->program->release();
        } else {
            qFatal("DRM_OpenGL_RenderMapper: unsupported format");
        }

        auto err = glGetError();
        if (err != GL_NO_ERROR) {
            qWarning("DRM_OpenGL_RenderMapper: OpenGL error: %s", gluErrorString(err));
        }
    }

    void DRM_OpenGL_RenderMapper::mapFrame() {
        Q_D(DRM_OpenGL_RenderMapper);

        LOOKUP_FUNCTION(PFNEGLCREATEIMAGEKHRPROC, eglCreateImageKHR);
        LOOKUP_FUNCTION(PFNEGLDESTROYIMAGEKHRPROC, eglDestroyImageKHR);
        LOOKUP_FUNCTION(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC, glEGLImageTargetTexture2DOES);

        auto *frameDescriptor = reinterpret_cast<AVDRMFrameDescriptor *>(d->currentFrame->data[0]);
        if (frameDescriptor->nb_layers != 1) {
            qWarning("DRM_OpenGL_RenderMapper: only support one layer");
            return;
        }
        if (frameDescriptor->layers[0].format == DRM_FORMAT_ARGB8888) {
            qDebug("RGB DRM frame");
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, d->textures[0]);
            eglDestroyImageKHR(d->eglDisplay, d->eglImages[0]);

            const EGLint attribs[] = {
                    EGL_WIDTH, d->currentFrame->width,
                    EGL_HEIGHT, d->currentFrame->height,
                    EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,
                    EGL_DMA_BUF_PLANE0_FD_EXT, frameDescriptor->objects[frameDescriptor->layers[0].planes[0].object_index].fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(frameDescriptor->layers[0].planes[0].offset),
                    EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(frameDescriptor->layers[0].planes[0].pitch),
                    EGL_NONE};
            d->eglImages[0] = eglCreateImageKHR(d->eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);
            auto error = eglGetError();
            if (d->eglImages[0] == EGL_NO_IMAGE_KHR || error != EGL_SUCCESS) {
                qWarning("DRM_OpenGL_RenderMapper: eglCreateImageKHR failed: %s", eglErrorString(error).c_str());
                return;
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, d->textures[0]);
            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, d->eglImages[0]);
            glBindTexture(GL_TEXTURE_2D, 0);
            auto err = glGetError();
            if (err != GL_NO_ERROR) {
                qWarning("DRM_OpenGL_RenderMapper: OpenGL error: %s", gluErrorString(err));
            }
        } else if (frameDescriptor->layers[0].format == DRM_FORMAT_NV12 || frameDescriptor->layers[0].format == DRM_FORMAT_P010) {
            qDebug("YUV DRM frame");
            for (uint planeIdx = 0; planeIdx < 2; ++planeIdx) {
                glActiveTexture(GL_TEXTURE0 + planeIdx);
                glBindTexture(GL_TEXTURE_2D, d->textures[planeIdx]);
                eglDestroyImageKHR(d->eglDisplay, d->eglImages[planeIdx]);

                const EGLint attribs[] = {
                        EGL_WIDTH, d->currentFrame->width,
                        EGL_HEIGHT, d->currentFrame->height,
                        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,
                        EGL_DMA_BUF_PLANE0_FD_EXT, frameDescriptor->objects[frameDescriptor->layers[0].planes[0].object_index].fd,
                        EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(frameDescriptor->layers[0].planes[0].offset),
                        EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(frameDescriptor->layers[0].planes[0].pitch),
                        EGL_DMA_BUF_PLANE1_FD_EXT, frameDescriptor->objects[frameDescriptor->layers[0].planes[1].object_index].fd,
                        EGL_DMA_BUF_PLANE1_OFFSET_EXT, static_cast<EGLint>(frameDescriptor->layers[0].planes[1].offset),
                        EGL_DMA_BUF_PLANE1_PITCH_EXT, static_cast<EGLint>(frameDescriptor->layers[0].planes[1].pitch),
                        EGL_NONE};
                d->eglImages[planeIdx] = eglCreateImageKHR(d->eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);
                auto error = eglGetError();
                if (d->eglImages[planeIdx] == EGL_NO_IMAGE_KHR || error != EGL_SUCCESS) {
                    qWarning("DRM_OpenGL_RenderMapper: eglCreateImageKHR failed: %s", eglErrorString(error).c_str());
                    return;
                }
                glActiveTexture(GL_TEXTURE0 + planeIdx);
                glBindTexture(GL_TEXTURE_2D, d->textures[planeIdx]);
                glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, d->eglImages[planeIdx]);
                glBindTexture(GL_TEXTURE_2D, 0);
                auto err = glGetError();
                if (err != GL_NO_ERROR) {
                    qWarning("DRM_OpenGL_RenderMapper: OpenGL error: %s", gluErrorString(err));
                }
            }
        }
    }

    void DRM_OpenGL_RenderMapper::start() {
        Q_D(DRM_OpenGL_RenderMapper);
        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            if (d->context->thread() != this) {
                d->context->moveToThread(this);
            }
            QThread::start();
        }
    }

    void DRM_OpenGL_RenderMapper::stop() {
        Q_D(DRM_OpenGL_RenderMapper);
        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->afterStopThread = QThread::currentThread();
            QThread::quit();
            d->frameProcessed.wakeAll();
            d->frameAvailable.wakeAll();
            d->fboPool.reset();// Delete framepool to unlock waiting thread
            QThread::wait();
            QMutexLocker locker(&d->inputQueueMutex);
            d->inputQueue.clear();
        }
    }

    void DRM_OpenGL_RenderMapper::enqueueFrame(const std::shared_ptr<AVFrame> &frame) {
        Q_D(DRM_OpenGL_RenderMapper);
        if (frame->format == AV_PIX_FMT_DRM_PRIME) {
            auto *frameDescriptor = reinterpret_cast<AVDRMFrameDescriptor *>(frame->data[0]);
            if (frameDescriptor->nb_layers != 1) {
                qWarning("DRM_OpenGL_RenderMapper: DRM frame with %d layers", frameDescriptor->nb_layers);
                return;
            }
            if (frameDescriptor->layers[0].format != DRM_FORMAT_NV12 &&
                frameDescriptor->layers[0].format != DRM_FORMAT_P010 &&
                frameDescriptor->layers[0].format != DRM_FORMAT_ARGB8888) {
                qWarning("DRM_OpenGL_RenderMapper: DRM frame with unsupported format %d", frameDescriptor->layers[0].format);
                return;
            }
            const auto &queueFrame = frame;

            QMutexLocker locker(&d->inputQueueMutex);
            if (d->inputQueue.size() > 4) {
                d->frameProcessed.wait(&d->inputQueueMutex);
                if (d->inputQueue.size() > 4) {
                    qWarning("DRM_OpenGL_RenderMapper: input queue is full");
                    return;
                }
            }
            d->inputQueue.enqueue(queueFrame);
            d->frameAvailable.wakeOne();
        }
    }

    void DRM_OpenGL_RenderMapper::run() {
        Q_D(DRM_OpenGL_RenderMapper);

        if (!d->running) {
            return;
        }

        while (d->running) {
            QMutexLocker locker(&d->inputQueueMutex);
            if (d->inputQueue.isEmpty()) {
                d->frameAvailable.wait(&d->inputQueueMutex, 200);
                if (!d->running) {
                    return;
                }
                if (d->inputQueue.isEmpty()) {
                    continue;
                }
            }
            auto frame = d->inputQueue.dequeue();
            locker.unlock();
            d->frameProcessed.notify_one();

            bool firstFrame;

            {
                QMutexLocker frameLocker(&d->currentFrameMutex);
                firstFrame = !d->currentFrame;
                d->currentFrame = frame;
            }

            d->context->makeCurrent(d->surface.get());
            if (firstFrame) {
                initializeInterop();

                qDebug("First frame");
            }

            if (!d->fboPool) {
                d->fboPool = std::make_unique<common::FBOPool>(QSize(d->currentFrame->width, d->currentFrame->height), true, 4, 12);
            }

            mapFrame();
            qDebug("Mapped frame");

            auto fbo = d->fboPool->getFBO(1000);
            if (!fbo) {
                qWarning("DRM_OpenGL_RenderMapper: failed to get FBO");
                goto end;
            }

            fbo->bind();
            glViewport(0, 0, d->currentFrame->width, d->currentFrame->height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            d->bindResources();
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
            d->releaseResources();
            QOpenGLFramebufferObject::bindDefault();
            d->context->doneCurrent();

            emit frameReady(d->currentFrame->pts, fbo);
            qDebug("Frame ready");
        }
    end:
        if (d->afterStopThread) {
            d->context->moveToThread(d->afterStopThread);
        }
        qDebug("Render thread stopped");
    }

    void DRM_OpenGL_RenderMapperPrivate::bindResources() {
        program->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        if (textures[1]) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, textures[1]);
        }

        vao->bind();
        ibo->bind();
    }

    void DRM_OpenGL_RenderMapperPrivate::releaseResources() {
        vao->release();
        vbo->release();
        program->release();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (textures[1]) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void DRM_OpenGL_RenderMapperPrivate::destroyResources() {
        {
            QMutexLocker locker(&currentFrameMutex);
            if (currentFrame) {
                currentFrame.reset();
            }
        }

        if (!inputQueue.isEmpty()) {
            QMutexLocker locker(&inputQueueMutex);
            inputQueue.clear();
        }

        program.reset();

        if (ibo) {
            ibo->destroy();
            ibo.reset();
        }
        if (vbo) {
            vbo->destroy();
            vbo.reset();
        }
        if (vao) {
            vao->destroy();
            vao.reset();
        }

        LOOKUP_FUNCTION(PFNEGLDESTROYIMAGEKHRPROC, eglDestroyImageKHR);
        if (eglImages[0]) {
            eglDestroyImageKHR(eglDisplay, eglImages[0]);
            eglImages[0] = EGL_NO_IMAGE_KHR;
        }
        if (eglImages[1]) {
            eglDestroyImageKHR(eglDisplay, eglImages[1]);
            eglImages[1] = EGL_NO_IMAGE_KHR;
        }

        fboPool.reset();
    }
}// namespace AVQt
