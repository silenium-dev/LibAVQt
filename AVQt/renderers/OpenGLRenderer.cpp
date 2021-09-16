//
// Created by silas on 3/21/21.
//

#include "OpenGLRenderer.h"
#include "renderers/private/OpenGLRenderer_p.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <GL/GLU.h>

static void loadResources() {
    Q_INIT_RESOURCE(resources);
}

#include <QtOpenGL>

namespace AVQt {
    OpenGLRenderer::OpenGLRenderer(QWidget *parent) : QObject(parent), QOpenGLFunctions(),
                                                      d_ptr(new OpenGLRendererPrivate(this)) {
        Q_D(AVQt::OpenGLRenderer);
        bool shouldBe = false;
        if (OpenGLRendererPrivate::resourcesLoaded.compare_exchange_strong(shouldBe, true)) {
            loadResources();
        }
    }

    [[maybe_unused]] [[maybe_unused]] OpenGLRenderer::OpenGLRenderer(OpenGLRendererPrivate &p) : d_ptr(&p) {
    }

    OpenGLRenderer::OpenGLRenderer(OpenGLRenderer &&other) noexcept : d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    OpenGLRenderer::~OpenGLRenderer() noexcept {
        Q_D(AVQt::OpenGLRenderer);
        d->destroyResources();
        delete d->m_clock;
        delete d_ptr;
    }

    int OpenGLRenderer::init(IFrameSource *source, AVRational framerate, int64_t duration) {
        Q_UNUSED(framerate)// Don't use framerate, because it is not always specified in stream information
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        d->m_duration = OpenGLRendererPrivate::timeFromMillis(duration);
        qDebug() << "Duration:" << d->m_duration.toString("hh:mm:ss.zzz");

        d->m_clock = new RenderClock;
        d->m_clock->setInterval(16);

        return 0;
    }

    int OpenGLRenderer::deinit(IFrameSource *source) {
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

    int OpenGLRenderer::start(IFrameSource *source) {
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

    int OpenGLRenderer::stop(IFrameSource *source) {
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

    void OpenGLRenderer::pause(IFrameSource *source, bool pause) {
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

    bool OpenGLRenderer::isPaused() {
        Q_D(AVQt::OpenGLRenderer);

        return d->m_paused.load();
    }

    void OpenGLRenderer::onFrame(IFrameSource *source, AVFrame *frame, int64_t duration, AVBufferRef *pDeviceCtx) {
        Q_D(AVQt::OpenGLRenderer);
        Q_UNUSED(source)
        Q_UNUSED(duration)
        d->onFrame(frame, pDeviceCtx);
    }

    void OpenGLRenderer::initializeGL(QOpenGLContext *context, QSurface *surface) {
        Q_D(AVQt::OpenGLRenderer);

        d->m_context = context;
        d->m_surface = surface;

        initializeOpenGLFunctions();

        d->initializePlatformAPI();
        d->initializeGL(context);
    }

    void OpenGLRenderer::paintGL() {
        Q_D(AVQt::OpenGLRenderer);
        //        auto t1 = std::chrono::high_resolution_clock::now();

        //        if (d->m_currentFrame) {
        //            int display_width = d->m_parent->width();
        //            int display_height = (d->m_parent->width() * d->m_currentFrame->height + d->m_currentFrame->width / 2) / d->m_currentFrame->width;
        //            if (display_height > d->m_parent->height()) {
        //                display_width = (d->m_parent->height() * d->m_currentFrame->width + d->m_currentFrame->height / 2) / d->m_currentFrame->height;
        //                display_height = d->m_parent->height();
        //            }
        //            qDebug("Viewport (x:%d, y:%d, w:%d, h:%d)", (d->m_parent->width() - display_width) / 2, (d->m_parent->height() - display_height) / 2, display_width, display_height);
        //            glViewport((d->m_parent->width() - display_width) / 2, (d->m_parent->height() - display_height) / 2, display_width, display_height);
        //        }

        //         Clear background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        //        qDebug("paintGL() – Running: %d", d->m_running.load());

        if (d->m_running.load()) {
            if (!d->m_paused.load()) {
                if (d->m_renderQueue.size() >= 2) {
                    auto timestamp = d->m_clock->getTimestamp();
                    if (timestamp >= d->m_renderQueue.first().result()->pts) {
                        d->m_updateRequired.store(true);
                        d->m_updateTimestamp.store(timestamp);
                    }
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
                        frame = d->m_renderQueue.dequeue();
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
                        d->initializeOnFirstFrame();
                        qDebug("First frame");
                    }
                    if (differentPixFmt) {
                        d->updatePixelFormat();
                    }
                    d->mapFrame();
                    auto err = glGetError();
                    if (err != GL_NO_ERROR) {
                        qFatal("Error in OpenGL: %s", gluErrorStringWIN(err));
                    }
                    qDebug("Mapped frame");
                }
                if (d->m_clock) {
                    if (!d->m_clock->isActive() && d->m_renderQueue.size() > 2) {
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

        //            if (!d->m_clock) {
        //                d->m_clock = new RenderClock;
        //                d->m_clock->setInterval((d->m_currentFrameTimeout < 0 ? 1 : d->m_currentFrameTimeout));
        //                connect(d->m_clock, &RenderClock::timeout, this, &OpenGLRenderer::triggerUpdate);
        //                qDebug("starting timer");
        //                d->m_clock->start();
        //            }

        if (d->m_currentFrame) {
            qDebug("Drawing frame with PTS: %lld", static_cast<long long>(d->m_currentFrame->pts));

            d->bindResources();
            auto err = glGetError();
            if (err != GL_NO_ERROR) {
                qFatal("Error in OpenGL: %s", gluErrorStringWIN(err));
            }
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
            err = glGetError();
            if (err != GL_NO_ERROR) {
                qFatal("Error in OpenGL: %s", gluErrorStringWIN(err));
            }
            d->releaseResources();
        }
    }
    QSize OpenGLRenderer::getFrameSize() {
        Q_D(AVQt::OpenGLRenderer);
        if (d->m_currentFrame) {
            return {d->m_currentFrame->width, d->m_currentFrame->height};
        } else {
            return {};
        }
    }
    //    void OpenGLRenderer::resized(QSize size) {
    //        Q_D(AVQt::OpenGLRenderer);
    //        d->updateSize(size);
    //    }
}// namespace AVQt