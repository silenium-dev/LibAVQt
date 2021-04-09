//
// Created by silas on 3/21/21.
//

#include "private/OpenGLRenderer_p.h"
#include "OpenGLRenderer.h"

#include <QtGui>

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

static void loadResources() {
    Q_INIT_RESOURCE(resources);
}

namespace AVQt {
    OpenGLRenderer::OpenGLRenderer(QWindow *parent) : QOpenGLWindow(QOpenGLWindow::NoPartialUpdate, parent),
                                                      d_ptr(new OpenGLRendererPrivate(this)) {
    }

    [[maybe_unused]] [[maybe_unused]] OpenGLRenderer::OpenGLRenderer(OpenGLRendererPrivate &p) : d_ptr(&p) {
    }

    OpenGLRenderer::~OpenGLRenderer() noexcept {
        Q_D(AVQt::OpenGLRenderer);

    }

    int OpenGLRenderer::init(IFrameSource *source, AVRational framerate, int64_t duration) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);
        int64_t h = duration / 3600000;
        int64_t m = (duration - h * 3600000) / 60000;
        int64_t s = (duration - h * 3600000 - m * 60000) / 1000;
        int64_t ms = duration - h * 3600000 - m * 60000 - s * 1000;

        d->m_duration = QTime(h, m, s, ms);
        d->m_position = QTime(0, 0, 0, 0);
        qDebug() << "Duration:" << d->m_duration.toString("hh:mm:ss");

        d->m_clock = new RenderClock;
        d->m_clock->setInterval(av_q2d(av_inv_q(framerate)) * 1000);
        connect(d->m_clock, &RenderClock::timeout, this, &OpenGLRenderer::triggerUpdate);

        return 0;
    }

    int OpenGLRenderer::deinit(IFrameSource *source) {
        Q_UNUSED(source)

        return 0;
    }

    int OpenGLRenderer::start(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        bool stopped = false;
        if (d->m_running.compare_exchange_strong(stopped, true)) {
            d->m_paused.store(false);
            qDebug("Started renderer");

            showNormal();
            requestActivate();
            qDebug("starting timer");
            d->m_clock->start();

            update();
            started();
        }
        return 0;
    }

    int OpenGLRenderer::stop(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        bool shouldBeCurrent = true;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, false)) {
            hide();

            d->m_clock->stop();

            {
                QMutexLocker lock(&d->m_renderQueueMutex);

                for (auto &e: d->m_renderQueue) {
                    av_frame_unref(e.first);
                }

                d->m_renderQueue.clear();
            }

            makeCurrent();

            delete d->m_program;

            d->m_ibo.destroy();
            d->m_vbo.destroy();
            d->m_vao.destroy();

            if (d->m_yTexture) {
                d->m_yTexture->destroy();
                delete d->m_uvTexture;
            }

            if (d->m_uvTexture) {
                d->m_uvTexture->destroy();
                delete d->m_uvTexture;
            }

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
            d->m_clock->pause(pause);
            qDebug("pause() called");
            paused(pause);
            update();
        }
    }

    bool OpenGLRenderer::isPaused() {
        Q_D(AVQt::OpenGLRenderer);

        return d->m_paused.load();
    }

    void OpenGLRenderer::onFrame(IFrameSource *source, AVFrame *frame, int64_t duration) {
        Q_UNUSED(source);

        Q_D(AVQt::OpenGLRenderer);

        QPair<AVFrame *, int64_t> newFrame;

        newFrame.first = av_frame_alloc();
//        av_frame_ref(newFrame.first, frame);
        av_hwframe_transfer_data(newFrame.first, frame, 0);
//        av_frame_unref(frame);

//        char strBuf[64];
        //qDebug() << "Pixel format:" << av_get_pix_fmt_string(strBuf, 64, static_cast<AVPixelFormat>(frame->format));

        newFrame.second = duration;

        while (d->m_renderQueue.size() >= 100) {
            QThread::msleep(4);
        }

        QMutexLocker lock(&d->m_renderQueueMutex);
        d->m_renderQueue.enqueue(newFrame);
    }

    void OpenGLRenderer::initializeGL() {
        Q_D(AVQt::OpenGLRenderer);

        loadResources();

        d->m_program = new QOpenGLShaderProgram();

        if (!d->m_program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/texture.vsh")) {
            qDebug() << "Vertex shader errors:\n" << d->m_program->log();
        }

        if (!d->m_program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/texture.fsh")) {
            qDebug() << "Fragment shader errors:\n" << d->m_program->log();
        }

        d->m_program->bindAttributeLocation("vertex", OpenGLRendererPrivate::PROGRAM_VERTEX_ATTRIBUTE);
        d->m_program->bindAttributeLocation("texCoord", OpenGLRendererPrivate::PROGRAM_TEXCOORD_ATTRIBUTE);

        if (!d->m_program->link()) {
            qDebug() << "Shader linkers errors:\n" << d->m_program->log();
        }

        d->m_program->bind();
        d->m_program->setUniformValue("textureY", 0);
        d->m_program->setUniformValue("textureUV", 1);
        d->m_program->release();

        float vertices[] = {
                1, 1, 0,   // top right
                1, -1, 0,   // bottom right
                -1, -1, 0,  // bottom left
                -1, 1, 0   // top left
        };

        float vertTexCoords[] = {
                0, 0,
                1, 1,
                0, 1,
                1, 0
        };

//    QColor vertexColors[] = {
//        QColor(0xf6, 0xa5, 0x09, 128),
//        QColor(0xcb, 0x2d, 0xde, 128),av_q2d(av_inv_q(framerate))
//        QColor(0x0e, 0xee, 0xd1, 128),
//        QColor(0x06, 0x89, 0x18, 128)
//    };

        std::vector<float> vertexBufferData(5 * 4);  // 8 entries per vertex * 4 vertices

        float *buf = vertexBufferData.data();

        for (int v = 0; v < 4; ++v, buf += 5) {
            buf[0] = vertices[3 * v];
            buf[1] = vertices[3 * v + 1];
            buf[2] = vertices[3 * v + 2];

            buf[3] = vertTexCoords[v];
            buf[4] = vertTexCoords[v + 1];
        }

        d->m_vbo = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        d->m_vbo.create();
        d->m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        d->m_vbo.bind();

        d->m_vbo.allocate(vertexBufferData.data(), vertexBufferData.size() * sizeof(float));

        d->m_vao.create();
        d->m_vao.bind();

        uint indices[] = {
                0, 1, 3, // first tri
                1, 2, 3  // second tri
        };

        d->m_ibo = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
        d->m_ibo.create();
        d->m_ibo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        d->m_ibo.bind();
        d->m_ibo.allocate(indices, sizeof(indices));


        int stride = 5 * sizeof(float);

        // layout location 0 - vec3 with coords
        d->m_program->enableAttributeArray(0);
        d->m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);

        // layout location 1 - vec3 with colors
//    d->m_program->enableAttributeArray(1);
//    int colorOffset = 3 * sizeof(float);
//    d->m_program->setAttributeBuffer(1, GL_FLOAT, colorOffset, 3, stride);

        // layout location 1 - vec2 with texture coordinates
        d->m_program->enableAttributeArray(1);
        int texCoordsOffset = 3 * sizeof(float);
        d->m_program->setAttributeBuffer(1, GL_FLOAT, texCoordsOffset, 2, stride);

//                // layout location 2 - int with texture id
//                d->m_program->enableAttributeArray(2);
//                d->m_program->setAttributeValue(2, d->m_yTexture->textureId());

        // Release (unbind) all

        d->m_vbo.release();
        d->m_vao.release();
    }

    void OpenGLRenderer::paintGL() {
        Q_D(AVQt::OpenGLRenderer);
//        auto t1 = std::chrono::high_resolution_clock::now();

//         Clear background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

//        qDebug("paintGL() – Running: %d", d->m_running.load());

        if (d->m_running.load()) {
            if (!d->m_paused.load()) {
                if (d->m_clock) {
                    if (!d->m_clock->isActive() && d->m_renderQueue.size() > 50) {
                        d->m_clock->start();
                    }
                }
                if (d->m_updateRequired.load() && !d->m_renderQueue.isEmpty()) {
                    d->m_updateRequired.store(false);
//                    qDebug("Adding duration %ld ms to position", d->m_currentFrameTimeout);
                    d->m_position = d->m_position.addMSecs(d->m_currentFrameTimeout);
                    QPair<AVFrame *, int64_t> frame;
                    {
                        QMutexLocker lock2(&d->m_renderQueueMutex);
                        frame = d->m_renderQueue.dequeue();
                    }

                    bool firstFrame = true;
                    bool differentPixFmt = true;

                    {
                        QMutexLocker lock(&d->m_currentFrameMutex);
                        firstFrame = !d->m_currentFrame;
                        if (!firstFrame) {
                            if (d->m_currentFrame->format == frame.first->format) {
                                differentPixFmt = false;
                            }
                        }

                        if (d->m_currentFrame) {
                            if (d->m_currentFrame->format == AV_PIX_FMT_BGRA) {
                                av_freep(&d->m_currentFrame->data[0]);
                                av_frame_free(&d->m_currentFrame);
                                d->m_currentFrame = nullptr;
                            } else {
                                av_frame_unref(d->m_currentFrame);
                                av_frame_free(&d->m_currentFrame);
                                d->m_currentFrame = nullptr;
                            }
                        }

                        d->m_currentFrame = frame.first;
                        d->m_currentFrameTimeout = frame.second;
                    }

                    if (firstFrame) {
                        d->m_yTexture = new QOpenGLTexture(
                                QImage(d->m_currentFrame->width, d->m_currentFrame->height, QImage::Format_ARGB32));
                        d->m_uvTexture = new QOpenGLTexture(
                                QImage(d->m_currentFrame->width / 2, d->m_currentFrame->height / 2, QImage::Format_ARGB32));

                        d->m_yTexture->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::LinearMipMapLinear);
                        d->m_uvTexture->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::LinearMipMapLinear);
                    }
//                    qDebug("Frame duration: %ld ms", d->m_currentFrameTimeout);
                    if (differentPixFmt) {
                        d->m_program->bind();
                    }
                    d->m_yTexture->bind(0);
                    d->m_uvTexture->bind(1);
                    switch (d->m_currentFrame->format) {
                        case AV_PIX_FMT_BGRA:
                            d->m_yTexture->setData(QOpenGLTexture::PixelFormat::BGRA, QOpenGLTexture::UInt8,
                                                   d->m_currentFrame->data[0]);
                            if (differentPixFmt) {
                                d->m_program->setUniformValue("inputFormat", 0);
                            }
                            break;
                        case AV_PIX_FMT_YUV420P:
                        case AV_PIX_FMT_NV12:
                            d->m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, d->m_currentFrame->data[0]);
                            d->m_uvTexture->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt8, d->m_currentFrame->data[1]);
                            if (differentPixFmt) {
                                d->m_program->setUniformValue("inputFormat", 1);
                            }
                            break;
                        case AV_PIX_FMT_YUV420P10:
                        case AV_PIX_FMT_P010:
                            d->m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16, d->m_currentFrame->data[0]);
                            d->m_uvTexture->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt16, d->m_currentFrame->data[1]);
                            if (differentPixFmt) {
                                d->m_program->setUniformValue("inputFormat", 1);
                            }
                            break;
                        default:
                            qFatal("Pixel format not supported");
                    }
                    if (differentPixFmt) {
                        d->m_program->release();
                    }
                }
            } else if (d->m_clock) {
                if (d->m_clock->isActive()) {
                    d->m_clock->stop();
                }
            }

//            if (!d->m_clock) {
//                d->m_clock = new RenderClock;
//                d->m_clock->setInterval((d->m_currentFrameTimeout < 0 ? 1 : d->m_currentFrameTimeout));
//                connect(d->m_clock, &RenderClock::timeout, this, &OpenGLRenderer::triggerUpdate);
//                qDebug("starting timer");
//                d->m_clock->start();
//            }

            if (d->m_clock->getInterval() != d->m_currentFrameTimeout) {
                qDebug("Resetting timer %ld != %ld", d->m_clock->getInterval(), d->m_currentFrameTimeout);
                d->m_clock->setInterval((d->m_currentFrameTimeout < 0 ? 1 : d->m_currentFrameTimeout));
                d->m_clock->stop();
                d->m_clock->start();
            }

            if (d->m_currentFrame) {
                d->m_program->bind();
                if (!d->m_yTexture->isBound(0)) {
                    d->m_yTexture->bind(0);
                    d->m_uvTexture->bind(1);
                }

                d->m_vao.bind();
                d->m_ibo.bind();
//    d->m_program->enableAttributeArray(PROGRAM_VERTEX_ATTRIBUTE);
//    d->m_program->enableAttributeArray(PROGRAM_TEXCOORD_ATTRIBUTE);
//    d->m_program->setAttributeBuffer(PROGRAM_VERTEX_ATTRIBUTE, GL_FLOAT, 0, 3, 5 * sizeof(GLfloat));
//    d->m_program->setAttributeBuffer(PROGRAM_TEXCOORD_ATTRIBUTE, GL_FLOAT, 3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

                d->m_vao.release();
                d->m_vbo.release();
                d->m_program->release();
                d->m_yTexture->release(0);
                d->m_uvTexture->release(1);
            }

            int height = this->height();

            GLdouble model[4][4], proj[4][4];
            GLint view[4];
            glGetDoublev(GL_MODELVIEW_MATRIX, &model[0][0]);
            glGetDoublev(GL_PROJECTION_MATRIX, &proj[0][0]);
            glGetIntegerv(GL_VIEWPORT, &view[0]);
            GLdouble textPosX = 0, textPosY = 0, textPosZ = 0;

            d->project(-0.9f, 0.8f, 0.0f, &model[0][0], &proj[0][0], &view[0],
                       &textPosX, &textPosY, &textPosZ);

            textPosY = height - textPosY; // y is inverted

            QPainter p(this);

            p.setPen(QPen(Qt::black, 2.0));
            p.setFont(QFont("Roboto", 24));
            p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            QFont roboto("Roboto Condensed", 24);
            p.setFont(roboto);

//            qDebug() << "Current timestamp:" << d->m_position.toString("hh:mm:ss.zzz");

            QString overlay(d->m_position.toString("hh:mm:ss") + "/" + d->m_duration.toString("hh:mm:ss"));
            QFontMetrics fm(roboto);
            p.fillRect(fm.boundingRect(overlay).translated(textPosX, textPosY).adjusted(-5, -5, 5, 5), QColor(0xFF, 0xFF, 0xFF, 0x48));
            p.drawText(fm.boundingRect(overlay).translated(textPosX, textPosY), overlay);
            p.end();
//            auto t2 = std::chrono::high_resolution_clock::now();
            //qDebug("Render frametime: %ld us", std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
//            QTimer::singleShot(d->m_currentFrameTimeout - std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count(),
//                               [&] { update(); });
            if (!d->m_paused.load()) {
                update();
            }
        }
    }

    void OpenGLRenderer::mouseReleaseEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            pause(nullptr, !isPaused());
        }
    }

    void OpenGLRenderer::triggerUpdate() {
        Q_D(AVQt::OpenGLRenderer);

        d->m_updateRequired.store(true);
    }

    RenderClock *OpenGLRenderer::getClock() {
        Q_D(AVQt::OpenGLRenderer);
        return d->m_clock;
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

        if (in[3] == 0.0)
            return GL_FALSE;

        in[0] /= in[3];
        in[1] /= in[3];
        in[2] /= in[3];

        *winx = viewport[0] + (1 + in[0]) * viewport[2] / 2;
        *winy = viewport[1] + (1 + in[1]) * viewport[3] / 2;

        *winz = (1 + in[2]) / 2;
        return GL_TRUE;
    }

    void OpenGLRendererPrivate::transformPoint(GLdouble *out, const GLdouble *m, const GLdouble *in) {
#define M(row, col)  m[col*4+row]
        out[0] = M(0, 0) * in[0] + M(0, 1) * in[1] + M(0, 2) * in[2] + M(0, 3) * in[3];
        out[1] = M(1, 0) * in[0] + M(1, 1) * in[1] + M(1, 2) * in[2] + M(1, 3) * in[3];
        out[2] = M(2, 0) * in[0] + M(2, 1) * in[1] + M(2, 2) * in[2] + M(2, 3) * in[3];
        out[3] = M(3, 0) * in[0] + M(3, 1) * in[1] + M(3, 2) * in[2] + M(3, 3) * in[3];
#undef M
    }

}
