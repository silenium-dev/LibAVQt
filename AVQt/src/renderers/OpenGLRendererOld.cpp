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

#include "renderers/OpenGLRendererOld.hpp"
#include "renderers/private/OpenGLRendererOld_p.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <QOpenGLFramebufferObject>
#include <QtConcurrent>
#include <QtOpenGL>


static void loadResources() {
    Q_INIT_RESOURCE(AVQtShader);
}

namespace AVQt {
    OpenGLRendererOld::OpenGLRendererOld(QObject *parent) : QObject(parent), QOpenGLFunctions(),
                                                      d_ptr(new OpenGLRendererPrivate(this)) {
        Q_D(AVQt::OpenGLRenderer);
        bool shouldBe = false;
        if (OpenGLRendererPrivate::resourcesLoaded.compare_exchange_strong(shouldBe, true)) {
            loadResources();
        }
    }

    [[maybe_unused]] [[maybe_unused]] OpenGLRendererOld::OpenGLRendererOld(OpenGLRendererPrivate &p) : d_ptr(&p) {
    }

    OpenGLRendererOld::OpenGLRendererOld(OpenGLRendererOld &&other) noexcept : d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    OpenGLRendererOld::~OpenGLRendererOld() noexcept {
        Q_D(AVQt::OpenGLRenderer);
        d->destroyResources();
        delete d->m_clock;
        delete d_ptr;
    }

    int OpenGLRendererOld::init(IFrameSource *source, AVRational framerate, int64_t duration) {
        Q_UNUSED(framerate)// Don't use framerate, because it is not always specified in stream information
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        d->m_duration = OpenGLRendererPrivate::timeFromMillis(duration);
        qDebug() << "Duration:" << d->m_duration.toString("hh:mm:ss.zzz");

        d->m_clock = new RenderClock;
        d->m_clock->setInterval(16);

        return 0;
    }

    int OpenGLRendererOld::deinit(IFrameSource *source) {
        Q_D(AVQt::OpenGLRenderer);
        stop(source);

        if (d->m_pQSVDerivedDeviceContext) {
            av_buffer_unref(&d->m_pQSVDerivedFramesContext);
            av_buffer_unref(&d->m_pQSVDerivedDeviceContext);
        }

        delete d->m_clock;
        d->m_clock = nullptr;

        return 0;
    }

    int OpenGLRendererOld::start(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        bool stopped = false;
        if (d->m_running.compare_exchange_strong(stopped, true)) {
            d->m_paused.store(false);
            qDebug("Started renderer");

            started();
        }
        return 0;
    }

    int OpenGLRendererOld::stop(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        bool shouldBeCurrent = true;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, false)) {
            if (d->m_clock) {
                d->m_clock->stop();
            }

            d->destroyResources();

            stopped();
            return 0;
        }
        return -1;
    }

    void OpenGLRendererOld::pause(IFrameSource *source, bool pause) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        bool shouldBeCurrent = !pause;

        if (d->m_paused.compare_exchange_strong(shouldBeCurrent, pause)) {
            if (d->m_clock->isActive() == !pause) {
                d->m_clock->pause(pause);
            }
            qDebug("pause() called");
            paused(pause);
        }
    }

    bool OpenGLRendererOld::isPaused() {
        Q_D(AVQt::OpenGLRenderer);

        return d->m_paused.load();
    }

    void OpenGLRendererOld::initializeGL(QOpenGLContext *context) {
        Q_D(AVQt::OpenGLRenderer);

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

        d->m_program = new QOpenGLShaderProgram();
        d->m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
        d->m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);

        d->m_program->bindAttributeLocation("vertex", OpenGLRendererPrivate::PROGRAM_VERTEX_ATTRIBUTE);
        d->m_program->bindAttributeLocation("texCoord", OpenGLRendererPrivate::PROGRAM_TEXCOORD_ATTRIBUTE);

        if (!d->m_program->link()) {
            qDebug() << "Shader linkers errors:\n"
                     << d->m_program->log();
        }

        d->m_program->bind();
        d->m_program->setUniformValue("textureY", 0);
        d->m_program->setUniformValue("textureU", 1);
        d->m_program->setUniformValue("textureV", 2);
        d->m_program->release();
    }

    void OpenGLRendererOld::paintGL() {
        Q_D(AVQt::OpenGLRenderer);

        //        if (d->m_currentFrame) {
        //            int display_width = rect.width();
        //            int display_height = (rect.width() * d->m_currentFrame->height + d->m_currentFrame->width / 2) / d->m_currentFrame->width;
        //            if (display_height > rect.height()) {
        //                display_width = (rect.height() * d->m_currentFrame->width + d->m_currentFrame->height / 2) / d->m_currentFrame->height;
        //                display_height = rect.height();
        //            }
        //            glViewport((rect.width() - display_width) / 2, (rect.height() - display_height) / 2, display_width, display_height);
        //        }

        //         Clear background
        //        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        //        glClear(GL_COLOR_BUFFER_BIT);

        if (d->m_running.load()) {
            if (!d->m_paused.load()) {
                if (isUpdateRequired()) {
                    d->m_updateRequired.store(true);
                    d->m_updateTimestamp.store(d->m_clock->getTimestamp());
                }
                if (d->m_updateRequired.load() && d->m_renderQueue.size() >= 2) {
                    d->m_updateRequired.store(false);
                    auto frame = d->m_renderQueue.dequeue().result();
                    while (!d->m_renderQueue.isEmpty()) {
                        if (frame->pts <= d->m_updateTimestamp.load()) {
                            if (d->m_renderQueue.first().result()->pts >= d->m_updateTimestamp.load() || d->m_renderQueue.size() == 1) {
                                break;
                            } else {
                                qDebug("Discarding video frame at PTS: %lld < PTS: %lld", static_cast<long long>(frame->pts),
                                       d->m_updateTimestamp.load());
                                av_frame_free(&frame);
                            }
                        }
                        QMutexLocker lock2(&d->m_renderQueueMutex);
                        frame = d->m_renderQueue.dequeue().result();
                    }

                    bool firstFrame;
                    bool differentPixFmt = true;

                    {
                        QMutexLocker lock(&d->m_currentFrameMutex);
                        firstFrame = !d->m_currentFrame;
                        if (d->m_currentFrame) {
                            if (d->m_currentFrame->format == frame->format) {
                                differentPixFmt = false;
                            }
                        }

                        if (d->m_currentFrame) {
                            av_frame_free(&d->m_currentFrame);
                        }

                        d->m_currentFrame = frame;
                    }

                    if (!d->m_renderQueue.isEmpty()) {
                        frameProcessingStarted(d->m_currentFrame->pts, d->m_renderQueue.first().result()->pts - d->m_currentFrame->pts);
                    }
                    if (!d->m_renderQueue.isEmpty()) {
                        frameProcessingFinished(d->m_currentFrame->pts, d->m_renderQueue.first().result()->pts - d->m_currentFrame->pts);
                    }
                    if (firstFrame) {
                        initializeInterop();
                        qDebug("First frame");
                    }
                    if (differentPixFmt) {
                        d->updatePixelFormat();
                    }
                    mapFrame();
                    qDebug("Mapped frame");v
                }
                if (d->m_clock) {
                    if (!d->m_clock->isActive() && d->m_renderQueue.size() > 1) {
                        d->m_clock->start();
                    } else if (d->m_clock->isPaused()) {
                        d->m_clock->pause(false);
                    }
                }
            }
        } else if (d->m_clock) {
            if (d->m_clock->isActive()) {
                d->m_clock->pause(true);
            }
        }

        if (d->m_currentFrame) {
            qDebug("Drawing frame with PTS: %lld", static_cast<long long>(d->m_currentFrame->pts));

            d->bindResources();
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
            d->releaseResources();
        }
        glFinish();
    }

    void OpenGLRendererOld::paintFBO(QOpenGLFramebufferObject *fbo) {
        fbo->bind();
        glViewport(0, 0, fbo->width(), fbo->height());
        paintGL();
        QOpenGLFramebufferObject::bindDefault();
    }

    QSize OpenGLRendererOld::getFrameSize() {
        Q_D(AVQt::OpenGLRenderer);
        if (d->m_currentFrame) {
            return {d->m_currentFrame->width, d->m_currentFrame->height};
        } else if (!d->m_renderQueue.isEmpty()) {
            return {d->m_renderQueue.first().result()->width, d->m_renderQueue.first().result()->height};
        } else {
            return {};
        }
    }

    bool OpenGLRendererOld::isUpdateRequired() {
        Q_D(AVQt::OpenGLRenderer);
        if (!d->m_renderQueue.isEmpty()) {
            auto timestamp = d->m_clock->getTimestamp();
            if (timestamp >= d->m_renderQueue.first().result()->pts) {
                return true;
            }
        }
        return false;
    }

    void OpenGLRendererOld::cleanupGL() {
        Q_D(AVQt::OpenGLRenderer);
        d->destroyResources();
    }
}// namespace AVQt
