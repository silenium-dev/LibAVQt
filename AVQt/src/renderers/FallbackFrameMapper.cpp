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


#include "FallbackFrameMapper.hpp"
#include "private/FallbackFrameMapper_p.hpp"

#include "global.hpp"
#include "renderers/OpenGLFrameMapperFactory.hpp"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include <memory>

#ifndef Q_OS_ANDROID
#include <GL/gl.h>
#else
#include <GLES3/gl3.h>
#endif

#include <QFile>
#include <QOpenGLDebugLogger>

namespace AVQt {
    const api::OpenGLFrameMapperInfo &FallbackFrameMapper::info() {
        static const api::OpenGLFrameMapperInfo info{
                .metaObject = FallbackFrameMapper::staticMetaObject,
                .name = "SW",
                .platforms = {common::Platform::All},
                .supportedInputPixelFormats = {},
                .isSupported = []() { return true; },
        };
        return info;
    }

    FallbackFrameMapper::FallbackFrameMapper(QObject *parent)
        : QThread(parent), d_ptr(new FallbackFrameMapperPrivate(this)) {
        loadResources();
    }

    FallbackFrameMapper::~FallbackFrameMapper() {
        Q_D(FallbackFrameMapper);

        FallbackFrameMapper::stop();

        d->context->makeCurrent(d->surface.get());
        d->destroyResources();
        d->context->doneCurrent();
        delete d_ptr;
    }

    void FallbackFrameMapper::initializeGL(QOpenGLContext *context) {
        Q_D(FallbackFrameMapper);

        d->surface.reset(new QOffscreenSurface());
        d->surface->setFormat(context->format());
        d->surface->create();

        d->context = std::make_unique<QOpenGLContext>();
        d->context->setShareContext(context);
        d->context->setFormat(context->format());
        d->context->create();

        auto currentContext = QOpenGLContext::currentContext();

        d->context->makeCurrent(d->surface.get());

        initializeOpenGLFunctions();

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

        d->program = std::make_unique<QOpenGLShaderProgram>();
        d->program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
        d->program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);

        d->program->bindAttributeLocation("vertex", FallbackFrameMapperPrivate::PROGRAM_VERTEX_ATTRIBUTE);
        d->program->bindAttributeLocation("texCoord", FallbackFrameMapperPrivate::PROGRAM_TEXCOORD_ATTRIBUTE);

        if (!d->program->link()) {
            qDebug() << "Shader linkers errors:\n"
                     << d->program->log();
        }

        d->program->bind();
        d->program->setUniformValue("textureY", 0);
        d->program->setUniformValue("inputFormat", 0);
        d->program->release();

        float vertices[] = {
                1, 1, 0,  // top right
                1, -1, 0, // bottom right
                -1, -1, 0,// bottom left
                -1, 1, 0  // top left
        };

        float vertTexCoords[] = {
                0, 0, 1, 1,
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

        // layout location 1 - vec2 with textures coordinates
        d->program->enableAttributeArray(1);
        int texCoordsOffset = 3 * sizeof(float);
        d->program->setAttributeBuffer(1, GL_FLOAT, texCoordsOffset, 2, stride);

        // Release (unbind) all
        d->vbo.release();
        d->vao.release();

        d->context->doneCurrent();
        if (currentContext) {
            currentContext->makeCurrent(currentContext->surface());
        }
        d->context->moveToThread(this);
    }

    void FallbackFrameMapper::start() {
        Q_D(FallbackFrameMapper);

        if (isRunning() || !d->context) {
            return;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->firstFrame = true;
            QThread::start();
        }
    }

    void FallbackFrameMapper::stop() {
        Q_D(FallbackFrameMapper);

        if (!isRunning()) {
            return;
        }

        QMutexLocker lock(&d->renderMutex);
        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->afterStopThread = QThread::currentThread();
            lock.unlock();
            QThread::quit();
            QThread::wait();
            {
                QMutexLocker locker(&d->renderQueueMutex);
                d->renderQueue.clear();
            }
        } else {
            qWarning() << "Not running";
        }
    }

    void FallbackFrameMapper::enqueueFrame(const std::shared_ptr<AVFrame> &frame) {
        Q_D(FallbackFrameMapper);

        if (!frame) {
            return;
        }

        QMutexLocker locker(&d->renderQueueMutex);
        while (d->renderQueue.size() >= FallbackFrameMapperPrivate::RENDERQUEUE_MAX_SIZE) {
            locker.unlock();
            msleep(2);
            locker.relock();
        }
        d->renderQueue.enqueue(frame);
    }
    void FallbackFrameMapper::run() {
        Q_D(FallbackFrameMapper);

        while (d->running) {
            if (d->renderQueue.isEmpty()) {
                msleep(2);
                continue;
            }

            auto hwFrame = d->renderQueue.dequeue();
            if (!hwFrame) {
                continue;
            }

            QMutexLocker renderLock(&d->renderMutex);
            if (!d->running) {
                break;
            }

            std::shared_ptr<AVFrame> swFrame{nullptr, [](AVFrame *frame) {
                                                 if (frame) {
                                                     av_frame_free(&frame);
                                                 }
                                             }};
            if (hwFrame->hw_frames_ctx) {
                swFrame.reset(av_frame_alloc());
                if (!swFrame) {
                    qWarning() << "Failed to allocate frame";
                    continue;
                }
                if (0 != av_hwframe_transfer_data(swFrame.get(), hwFrame.get(), 0)) {
                    qWarning() << "Failed to transfer frame data";
                    continue;
                }
                swFrame->pts = hwFrame->pts;
                swFrame->width = hwFrame->width;
                swFrame->height = hwFrame->height;
            } else {
                swFrame = hwFrame;
            }

            if (!d->pSwsContext) {
                d->pSwsContext.reset(sws_getContext(swFrame->width, swFrame->height,
                                                    (AVPixelFormat) swFrame->format,
                                                    swFrame->width, swFrame->height,
                                                    AV_PIX_FMT_RGBA,
                                                    0, nullptr, nullptr, nullptr));
                if (!d->pSwsContext) {
                    qWarning() << "Failed to create sws context";
                    continue;
                }
            }

            //            switch (swFrame->format) {
            //                case AV_PIX_FMT_YUV420P10:
            //                case AV_PIX_FMT_YUV420P12:
            //                case AV_PIX_FMT_YUV420P14:
            //                case AV_PIX_FMT_YUV420P16:
            //                case AV_PIX_FMT_P010:
            //                case AV_PIX_FMT_P016:
            //                    textureFormat = QOpenGLTexture::RGBA16_UNorm;
            //                    pixelType = QOpenGLTexture::UInt16;
            //                    break;
            //                case AV_PIX_FMT_YUV420P:
            //                case AV_PIX_FMT_YUV422P:
            //                case AV_PIX_FMT_YUV444P:
            //                case AV_PIX_FMT_NV12:
            //                    textureFormat = QOpenGLTexture::RGBA8_UNorm;
            //                    pixelType = QOpenGLTexture::UInt8;
            //                    break;
            //                default:
            //                    qWarning() << "Unsupported pixel format:" << av_get_pix_fmt_name(static_cast<AVPixelFormat>(swFrame->format));
            //                    av_frame_free(&swFrame);
            //                    continue;
            //            }

            std::shared_ptr<AVFrame> frame{av_frame_alloc(), [](AVFrame *frame) {
                                               if (frame) {
                                                   av_frame_free(&frame);
                                               }
                                           }};
            if (!frame) {
                qWarning() << "Failed to allocate frame";
                continue;
            }
            frame->width = swFrame->width;
            frame->height = swFrame->height;
            frame->format = AV_PIX_FMT_RGBA;
            frame->pts = swFrame->pts;
            if (av_frame_get_buffer(frame.get(), 1) < 0) {
                qWarning() << "Failed to allocate frame buffer";
                continue;
            }
            int ret;
            if ((ret = sws_scale(d->pSwsContext.get(), swFrame->data, swFrame->linesize, 0, swFrame->height,
                                 frame->data, frame->linesize)) < 0) {
                char strBuf[256];
                qWarning() << "Failed to scale frame:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                continue;
            }

            //            QImage(frame->data[0], frame->width, frame->height, pixelType == QOpenGLTexture::UInt8 ? QImage::Format_ARGB32 : QImage::Format_RGBA64).save(QString("%1.png").arg(frame->pts));

            d->context->makeCurrent(d->surface.get());

            if (!d->texture) {
                d->texture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
                d->texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
                d->texture->setSize(frame->width, frame->height);
                d->texture->setAutoMipMapGenerationEnabled(false);
                d->texture->setMinificationFilter(QOpenGLTexture::Linear);
                d->texture->setMagnificationFilter(QOpenGLTexture::Linear);
                d->texture->setWrapMode(QOpenGLTexture::ClampToEdge);
                d->texture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
            }

            if (!d->fboPool) {
                d->fboPool = std::make_unique<common::FBOPool>(QSize(frame->width, frame->height), true, 4, 12);
            }

            d->texture->bind(0);
            d->texture->setData(0, 0, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, const_cast<const uint8_t *>(frame->data[0]));
            d->texture->release();

            auto fbo = d->fboPool->getFBO(1000);
            if (!fbo) {
                qWarning() << "Failed to get FBO";
                continue;
            }

            fbo->bind();

            glViewport(0, 0, frame->width, frame->height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            d->bindResources();
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
            d->releaseResources();

            fbo->release();
            d->context->doneCurrent();
            renderLock.unlock();
            emit frameReady(frame->pts, fbo);
            qDebug("Frame ready");
        }
        if (d->afterStopThread) {
            d->context->moveToThread(d->afterStopThread);
        }
    }

    void FallbackFrameMapperPrivate::bindResources() {
        texture->bind(0);
        program->bind();
        program->setUniformValue("textureY", 0);
        vao.bind();
        ibo.bind();
    }

    void FallbackFrameMapperPrivate::releaseResources() {
        vao.release();
        vbo.release();
        program->release();
        texture->release(0);
    }

    void FallbackFrameMapperPrivate::destroyResources() {
        program.reset();

        if (ibo.isCreated()) {
            ibo.destroy();
        }
        if (vbo.isCreated()) {
            vbo.destroy();
        }
        if (vao.isCreated()) {
            vao.destroy();
        }

        texture.reset();

        fboPool.reset();
    }

    void FallbackFrameMapperPrivate::destroyOffscreenSurface(QOffscreenSurface *surface) {
        if (surface) {
            surface->destroy();
            delete surface;
        }
    }

    void FallbackFrameMapperPrivate::destroySwsContext(SwsContext *swsContext) {
        if (swsContext) {
            sws_freeContext(swsContext);
        }
    }

    void FallbackFrameMapperPrivate::destroyQOpenGLTexture(QOpenGLTexture *texture) {
        if (texture) {
            texture->destroy();
            delete texture;
        }
    }
}// namespace AVQt

static_block {
    AVQt::OpenGLFrameMapperFactory::getInstance().registerRenderer(AVQt::FallbackFrameMapper::info());
}
