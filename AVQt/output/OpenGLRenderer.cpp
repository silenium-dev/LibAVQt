//
// Created by silas on 3/21/21.
//

#include "private/OpenGLRenderer_p.h"
#include "OpenGLRenderer.h"

#include <QtGui>
#include <QtConcurrent>
#include <QImage>

#include <va/va.h>
#include <va/va_drmcommon.h>
#include <libdrm/drm_fourcc.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glu.h>
#include <unistd.h>
#include <iostream>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "HidingNonVirtualFunction"
extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
}

#define LOOKUP_FUNCTION(type, func) \
        type func = (type) eglGetProcAddress(#func); \
        if (!(func)) { qFatal("eglGetProcAddress(" #func ")"); }

static void loadResources() {
    Q_INIT_RESOURCE(resources);
}

static int closefd(int fd) {
    return close(fd);
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
    OpenGLRenderer::OpenGLRenderer(QWidget *parent) : QOpenGLWidget(parent),
                                                      d_ptr(new OpenGLRendererPrivate(this)) {
        setAttribute(Qt::WA_QuitOnClose);
    }

    [[maybe_unused]] [[maybe_unused]] OpenGLRenderer::OpenGLRenderer(OpenGLRendererPrivate &p) : d_ptr(&p) {
    }

    OpenGLRenderer::OpenGLRenderer(OpenGLRenderer &&other) noexcept: d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    OpenGLRenderer::~OpenGLRenderer() noexcept {
        delete d_ptr;
    }

    int OpenGLRenderer::init(IFrameSource *source, AVRational framerate, int64_t duration) {
        Q_UNUSED(framerate) // Don't use framerate, because it is not always specified in stream information
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

        return 0;
    }

    int OpenGLRenderer::start(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        bool stopped = false;
        if (d->m_running.compare_exchange_strong(stopped, true)) {
            d->m_paused.store(false);
            qDebug("Started renderer");

            QMetaObject::invokeMethod(this, "showNormal", Qt::QueuedConnection);
//            QMetaObject::invokeMethod(this, "requestActivate", Qt::QueuedConnection);
            QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);

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

            if (d->m_currentFrame) {
                av_frame_free(&d->m_currentFrame);
            }

            if (d->m_clock) {
                d->m_clock->stop();
            }

            {
                QMutexLocker lock(&d->m_renderQueueMutex);

                for (auto &e: d->m_renderQueue) {
                    e.waitForFinished();
                    av_frame_unref(e.result());
                }

                d->m_renderQueue.clear();
            }

            makeCurrent();

            delete d->m_program;

            d->m_ibo.destroy();
            d->m_vbo.destroy();
            d->m_vao.destroy();

            delete d->m_yTexture;
            delete d->m_uTexture;
            delete d->m_vTexture;

            if (d->m_EGLImages[0]) {
                for (auto &EGLImage : d->m_EGLImages) {
                    eglDestroyImage(d->m_EGLDisplay, EGLImage);
                }
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
            if (d->m_clock->isActive()) {
                d->m_clock->pause(pause);
            }
            qDebug("pause() called");
            paused(pause);
            update();
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

        QMutexLocker onFrameLock{&d->m_onFrameMutex};

        QFuture<AVFrame *> queueFrame;

//        av_frame_ref(newFrame.first, frame);
        constexpr auto strBufSize = 64;
        char strBuf[strBufSize];
        qDebug("Pixel format: %s", av_get_pix_fmt_string(strBuf, 64, static_cast<AVPixelFormat>(frame->format)));
        switch (frame->format) {
//            case AV_PIX_FMT_QSV:
            case AV_PIX_FMT_CUDA:
            case AV_PIX_FMT_VDPAU:
            case AV_PIX_FMT_D3D11VA_VLD:
            case AV_PIX_FMT_DXVA2_VLD:
                qDebug("Transferring frame from GPU to CPU");
                queueFrame = QtConcurrent::run([](AVFrame *input) {
                    AVFrame *outFrame = av_frame_alloc();
                    int ret = av_hwframe_transfer_data(outFrame, input, 0);
                    if (ret != 0) {
                        char strBuf[64];
                        qFatal("%d: Could not transfer frame data: %s", ret, av_make_error_string(strBuf, 64, ret));
                    }
                    outFrame->pts = input->pts;
                    av_frame_free(&input);
                    return outFrame;
                }, av_frame_clone(frame));
                break;
            case AV_PIX_FMT_QSV: {
                qDebug("[AVQt::OpenGLRenderer] Mapping QSV frame to CPU for rendering");
                queueFrame = QtConcurrent::run([](AVFrame *input) {
                    AVFrame *outFrame = av_frame_alloc();
                    int ret = av_hwframe_map(outFrame, input, AV_HWFRAME_MAP_READ);
                    if (ret != 0) {
                        constexpr auto strBufSize = 64;
                        char strBuf[strBufSize];
                        qFatal("[AVQt::OpenGLRenderer] %d Could not map QSV frame to CPU: %s", ret,
                               av_make_error_string(strBuf, strBufSize, ret));
                    }
                    outFrame->pts = input->pts;
                    av_frame_free(&input);
                    return outFrame;
                }, av_frame_clone(frame));
                break;
            }
            default:
                qDebug("Referencing frame");
                queueFrame = QtConcurrent::run([d](AVFrame *input, AVBufferRef *pDeviceCtx) {
                    bool shouldBe = true;
                    if (d->m_firstFrame.compare_exchange_strong(shouldBe, false) && input->format == AV_PIX_FMT_VAAPI) {
                        d->m_pVAContext = static_cast<AVVAAPIDeviceContext *>(reinterpret_cast<AVHWDeviceContext *>(pDeviceCtx->data)->hwctx);
                        d->m_VADisplay = d->m_pVAContext->display;
                    }
                    return input;
                }, av_frame_clone(frame), pDeviceCtx);
                break;
        }

//        av_frame_unref(frame);

//        char strBuf[64];
        //qDebug() << "Pixel format:" << av_get_pix_fmt_string(strBuf, 64, static_cast<AVPixelFormat>(frame->format));


        while (d->m_renderQueue.size() > 6) {
            QThread::msleep(4);
        }

        QMutexLocker lock(&d->m_renderQueueMutex);
        d->m_renderQueue.enqueue(queueFrame);
    }

    void OpenGLRenderer::initializeGL() {
        Q_D(AVQt::OpenGLRenderer);

        loadResources();

        EGLint visual_attr[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                EGL_NONE
        };
        d->m_EGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
//        if (d->m_EGLDisplay == EGL_NO_DISPLAY) {
//            qDebug("Could not get default EGL display, connecting to X-Server");
//            Display *display = XOpenDisplay(nullptr);
//            if (!display) {
//                qFatal("Could not get X11 display");
//            }
//            d->m_EGLDisplay = eglGetDisplay(static_cast<EGLNativeDisplayType>(display));
        if (d->m_EGLDisplay == EGL_NO_DISPLAY) {
            qFatal("Could not get EGL display: %s", eglErrorString(eglGetError()).c_str());
        }
//        }
        if (!eglInitialize(d->m_EGLDisplay, nullptr, nullptr)) {
            qFatal("eglInitialize");
        }
        if (!eglBindAPI(EGL_OPENGL_API)) {
            qFatal("eglBindAPI");
        }

        EGLConfig cfg;
        EGLint cfg_count;
        if (!eglChooseConfig(d->m_EGLDisplay, visual_attr, &cfg, 1, &cfg_count) || (cfg_count < 1)) {
            qFatal("eglChooseConfig: %s", eglErrorString(eglGetError()).c_str());
        }
//        EGLint ctx_attr[] = {
//                EGL_CONTEXT_OPENGL_PROFILE_MASK,
//                EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
//                EGL_CONTEXT_MAJOR_VERSION, 3,
//                EGL_CONTEXT_MINOR_VERSION, 3,
//                EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
//                EGL_NONE
//        };
//        d->m_EGLContext = eglCreateContext(d->m_EGLDisplay, cfg, EGL_NO_CONTEXT, ctx_attr);
//        if (d->m_EGLContext == EGL_NO_CONTEXT) {
//            qFatal("eglCreateContext");
//        }

        qDebug("EGL Version: %s", eglQueryString(d->m_EGLDisplay, EGL_VERSION));

        QByteArray shaderVersionString;

        if (context()->isOpenGLES()) {
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
            qDebug() << "Shader linkers errors:\n" << d->m_program->log();
        }

        d->m_program->bind();
        d->m_program->setUniformValue("textureY", 0);
        d->m_program->setUniformValue("textureU", 1);
        d->m_program->setUniformValue("textureV", 2);
        d->m_program->release();
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-auto"

    void OpenGLRenderer::paintGL() {
        Q_D(AVQt::OpenGLRenderer);
//        auto t1 = std::chrono::high_resolution_clock::now();

        LOOKUP_FUNCTION(PFNEGLCREATEIMAGEKHRPROC, eglCreateImageKHR)
        LOOKUP_FUNCTION(PFNEGLDESTROYIMAGEKHRPROC, eglDestroyImageKHR)
        LOOKUP_FUNCTION(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC, glEGLImageTargetTexture2DOES)


        if (d->m_currentFrame) {
            int display_width = width();
            int display_height = (width() * d->m_currentFrame->height + d->m_currentFrame->width / 2) / d->m_currentFrame->width;
            if (display_height > height()) {
                display_width = (height() * d->m_currentFrame->width + d->m_currentFrame->height / 2) / d->m_currentFrame->height;
                display_height = height();
            }
            glViewport((width() - display_width) / 2, (height() - display_height) / 2, display_width, display_height);
        }

//         Clear background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

//        qDebug("paintGL() – Running: %d", d->m_running.load());

        if (d->m_running.load()) {
            if (!d->m_paused.load()) {
                if (d->m_clock) {
                    if (!d->m_clock->isActive() && d->m_renderQueue.size() > 2) {
                        d->m_clock->start();
                    } else if (d->m_clock->isPaused()) {
                        d->m_clock->pause(false);
                    }
                }
                if (!d->m_renderQueue.isEmpty()) {
                    auto timestamp = d->m_clock->getTimestamp();
                    if (timestamp >= d->m_renderQueue.first().result()->pts) {
                        d->m_updateRequired.store(true);
                        d->m_updateTimestamp.store(timestamp);
                    }
                }
                if (d->m_updateRequired.load() && !d->m_renderQueue.isEmpty()) {
                    d->m_updateRequired.store(false);
                    auto frame = d->m_renderQueue.dequeue().result();
                    while (!d->m_renderQueue.isEmpty()) {
                        if (frame->pts <= d->m_updateTimestamp.load()) {
                            if (d->m_renderQueue.first().result()->pts >= d->m_updateTimestamp.load()) {
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
                        if (!firstFrame) {
                            if (d->m_currentFrame->format == frame->format) {
                                differentPixFmt = false;
                            }
                        }

                        if (d->m_currentFrame) {
                            av_frame_free(&d->m_currentFrame);
                        }

                        d->m_currentFrame = frame;
                    }

                    if (firstFrame) {
                        // Frame has 64 pixel alignment, set max height coord to cut off additional pixels
                        float maxTexHeight = 1.0f;
                        if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI) {
                            VASurfaceID vaSurfaceId = reinterpret_cast<uintptr_t>(d->m_currentFrame->data[3]);
                            VAImage vaImage;
                            vaDeriveImage(d->m_VADisplay, vaSurfaceId, &vaImage);
                            maxTexHeight = static_cast<float>(d->m_currentFrame->height * 1.0 / (vaImage.height + 2.0));
                            vaDestroyImage(d->m_VADisplay, vaImage.image_id);
                        }

                        float vertices[] = {
                                1, 1, 0,   // top right
                                1, -1, 0,   // bottom right
                                -1, -1, 0,  // bottom left
                                -1, 1, 0   // top left
                        };

                        float vertTexCoords[] = {
                                0, 0,
                                maxTexHeight, maxTexHeight,
                                0, 1,
                                1, 0
                        };

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

                        d->m_vbo.allocate(vertexBufferData.data(), static_cast<int>(vertexBufferData.size() * sizeof(float)));

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

                        // layout location 1 - vec2 with texture coordinates
                        d->m_program->enableAttributeArray(1);
                        int texCoordsOffset = 3 * sizeof(float);
                        d->m_program->setAttributeBuffer(1, GL_FLOAT, texCoordsOffset, 2, stride);

                        // Release (unbind) all
                        d->m_vbo.release();
                        d->m_vao.release();
                        if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI) {
                            glGenTextures(2, d->m_textures);
                            for (const auto &texture : d->m_textures) {
                                glBindTexture(GL_TEXTURE_2D, texture);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            }
                            d->m_program->bind();
                            d->m_program->setUniformValue("inputFormat", 1);
                            d->m_program->release();
                        } else {
                            bool VTexActive = false, UTexActive = false;
                            QSize YSize, USize, VSize;
                            QOpenGLTexture::TextureFormat textureFormatY, textureFormatU, textureFormatV;
                            QOpenGLTexture::PixelFormat pixelFormatY, pixelFormatU, pixelFormatV;
                            QOpenGLTexture::PixelType pixelType;
                            switch (static_cast<AVPixelFormat>(d->m_currentFrame->format)) {
                                case AV_PIX_FMT_BGRA:
                                    YSize = QSize(d->m_currentFrame->width, d->m_currentFrame->height);
                                    textureFormatY = QOpenGLTexture::RGBA8_UNorm;
                                    pixelFormatY = QOpenGLTexture::BGRA;
                                    pixelType = QOpenGLTexture::UInt8;
                                    break;
                                case AV_PIX_FMT_YUV420P:
                                    YSize = QSize(d->m_currentFrame->width, d->m_currentFrame->height);
                                    USize = VSize = QSize(d->m_currentFrame->width / 2, d->m_currentFrame->height / 2);
                                    textureFormatY = textureFormatU = textureFormatV = QOpenGLTexture::R8_UNorm;
                                    pixelFormatY = pixelFormatU = pixelFormatV = QOpenGLTexture::Red;
                                    pixelType = QOpenGLTexture::UInt8;
                                    UTexActive = VTexActive = true;
                                    break;
                                case AV_PIX_FMT_YUV420P10:
                                    YSize = QSize(d->m_currentFrame->width, d->m_currentFrame->height);
                                    USize = VSize = QSize(d->m_currentFrame->width / 2, d->m_currentFrame->height / 2);
                                    textureFormatY = textureFormatU = textureFormatV = QOpenGLTexture::R16_UNorm;
                                    pixelFormatY = pixelFormatU = pixelFormatV = QOpenGLTexture::Red;
                                    pixelType = QOpenGLTexture::UInt16;
                                    UTexActive = VTexActive = true;
                                    break;
                                case AV_PIX_FMT_NV12:
                                    YSize = QSize(d->m_currentFrame->width, d->m_currentFrame->height);
                                    USize = QSize(d->m_currentFrame->width / 2, d->m_currentFrame->height / 2);
                                    textureFormatY = QOpenGLTexture::R8_UNorm;
                                    textureFormatU = QOpenGLTexture::RG8_UNorm;
                                    pixelFormatY = QOpenGLTexture::Red;
                                    pixelFormatU = QOpenGLTexture::RG;
                                    pixelType = QOpenGLTexture::UInt8;
                                    UTexActive = true;
                                    break;
                                case AV_PIX_FMT_P010:
                                    YSize = QSize(d->m_currentFrame->width, d->m_currentFrame->height);
                                    USize = QSize(d->m_currentFrame->width / 2, d->m_currentFrame->height / 2);
                                    textureFormatY = QOpenGLTexture::R16_UNorm;
                                    textureFormatU = QOpenGLTexture::RG16_UNorm;
                                    pixelFormatY = QOpenGLTexture::Red;
                                    pixelFormatU = QOpenGLTexture::RG;
                                    pixelType = QOpenGLTexture::UInt16;
                                    UTexActive = true;
                                    break;
                                default:
                                    qFatal("[AVQt::OpenGLRenderer] Unsupported pixel format");
                            }
                            d->m_yTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
                            d->m_yTexture->setSize(YSize.width(), YSize.height());
                            d->m_yTexture->setFormat(textureFormatY);
                            d->m_yTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                            d->m_yTexture->allocateStorage(pixelFormatY, pixelType);
                            if (UTexActive) {
                                d->m_uTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
                                d->m_uTexture->setSize(USize.width(), USize.height());
                                d->m_uTexture->setFormat(textureFormatU);
                                d->m_uTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                                d->m_uTexture->allocateStorage(pixelFormatU, pixelType);
                            }
                            if (VTexActive) {
                                d->m_vTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
                                d->m_vTexture->setSize(VSize.width(), VSize.height());
                                d->m_vTexture->setFormat(textureFormatV);
                                d->m_vTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                                d->m_vTexture->allocateStorage(pixelFormatV, pixelType);
                            }
                        }
                    }

                    if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI) {
                        for (int i = 0; i < 2; ++i) {
                            glActiveTexture(GL_TEXTURE0 + i);
                            glBindTexture(GL_TEXTURE_2D, d->m_textures[i]);
                            eglDestroyImageKHR(d->m_EGLDisplay, d->m_EGLImages[i]);
                        }

                        VASurfaceID va_surface = reinterpret_cast<uintptr_t>(d->m_currentFrame->data[3]);

//                        VAImage vaImage;
//                        vaDeriveImage(d->m_VADisplay, va_surface, &vaImage);
//                        VABufferInfo vaBufferInfo;
//                        memset(&vaBufferInfo, 0, sizeof(VABufferInfo));
//                        vaBufferInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_VA;
//                        vaAcquireBufferHandle(d->m_VADisplay, vaImage.buf, &vaBufferInfo);

                        VADRMPRIMESurfaceDescriptor prime;
                        if (vaExportSurfaceHandle(d->m_VADisplay, va_surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                                  VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS, &prime) !=
                            VA_STATUS_SUCCESS) {
                            qFatal("[AVQt::OpenGLRenderer] Could not export VA surface handle");
                        }
                        vaSyncSurface(d->m_VADisplay, va_surface);

                        static uint32_t formats[2];
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
                                qFatal("[AVQt::OpenGLRenderer] Invalid pixel format: %s",
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
                                    EGL_NONE
                            };

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
                            while (eglGetError() != EGL_SUCCESS);
                            d->m_EGLImages[i] = eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                                                  img_attr);

                            auto error = eglGetError();
                            if (!d->m_EGLImages[i] || d->m_EGLImages[i] == EGL_NO_IMAGE_KHR || error != EGL_SUCCESS) {
                                qFatal("[AVQt::OpenGLRenderer] Could not create %s EGLImage: %s", (i ? "UV" : "Y"),
                                       eglErrorString(error).c_str());
                            }
#undef LAYER
#undef PLANE

                            glActiveTexture(GL_TEXTURE0 + i);
                            glBindTexture(GL_TEXTURE_2D, d->m_textures[i]);
                            while (glGetError() != GL_NO_ERROR);
                            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, d->m_EGLImages[i]);
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
//                        vaReleaseBufferHandle(d->m_VADisplay, vaImage.buf);
//                        vaDestroyImage(d->m_VADisplay, vaImage.image_id);
                        for (int i = 0; i < (int) prime.num_objects; ++i) {
                            closefd(prime.objects[i].fd);
                        }
                    } else {
//                    qDebug("Frame duration: %ld ms", d->m_currentFrameTimeout);
                        if (differentPixFmt) {
                            d->m_program->bind();
                        }
                        d->m_yTexture->bind(0);
                        if (d->m_uTexture) {
                            d->m_uTexture->bind(1);
                        }
                        if (d->m_vTexture) {
                            d->m_vTexture->bind(2);
                        }
                        switch (d->m_currentFrame->format) {
                            case AV_PIX_FMT_BGRA:
                                d->m_yTexture->setData(QOpenGLTexture::PixelFormat::BGRA, QOpenGLTexture::UInt8,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[0]));
                                if (differentPixFmt) {
                                    d->m_program->setUniformValue("inputFormat", 0);
                                }
                                break;
                            case AV_PIX_FMT_NV12:
                                d->m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[0]));
                                d->m_uTexture->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt8,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[1]));
                                if (differentPixFmt) {
                                    d->m_program->setUniformValue("inputFormat", 1);
                                }
                                break;
                            case AV_PIX_FMT_P010:
                                d->m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[0]));
                                d->m_uTexture->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt16,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[1]));
                                if (differentPixFmt) {
                                    d->m_program->setUniformValue("inputFormat", 1);
                                }
                                break;
                            case AV_PIX_FMT_YUV420P:
                                d->m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[0]));
                                d->m_uTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[1]));
                                d->m_vTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[2]));
                                if (differentPixFmt) {
                                    d->m_program->setUniformValue("inputFormat", 2);
                                }
                                break;
                            case AV_PIX_FMT_YUV420P10:
                                d->m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[0]));
                                d->m_uTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[1]));
                                d->m_vTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16,
                                                       const_cast<const uint8_t *>(d->m_currentFrame->data[2]));
                                if (differentPixFmt) {
                                    d->m_program->setUniformValue("inputFormat", 3);
                                }
                                break;
                            default:
                                qFatal("Pixel format not supported");
                        }
                        if (differentPixFmt) {
                            d->m_program->release();
                        }
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
            d->m_program->bind();
            if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, d->m_textures[0]);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, d->m_textures[1]);
            } else {
                if (!d->m_yTexture->isBound(0)) {
                    d->m_yTexture->bind(0);
                    if (d->m_uTexture) {
                        d->m_uTexture->bind(1);
                    }
                    if (d->m_vTexture) {
                        d->m_vTexture->bind(2);
                    }
                }
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
            if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI) {

            } else {
                d->m_yTexture->release(0);
                if (d->m_uTexture) {
                    d->m_uTexture->release(1);
                }
                if (d->m_vTexture) {
                    d->m_vTexture->release(2);
                }
            }
        }

//        int height = this->height();

//        GLdouble model[4][4], proj[4][4];
//        GLint view[4];
//        glGetDoublev(GL_MODELVIEW_MATRIX, &model[0][0]);
//        glGetDoublev(GL_PROJECTION_MATRIX, &proj[0][0]);
//        glGetIntegerv(GL_VIEWPORT, &view[0]);
//        GLdouble textPosX = 0, textPosY = 0, textPosZ = 0;

//        OpenGLRendererPrivate::project(-0.9, 0.8, 0.0, &model[0][0], &proj[0][0], &view[0],
//                                       &textPosX, &textPosY, &textPosZ);

//        textPosY = height - textPosY; // y is inverted

        QPainter p(this);

        p.setPen(QPen(Qt::black, 2.0));
        p.setFont(QFont("Roboto", 24));
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        QFont roboto("Roboto Condensed", 24);
        p.setFont(roboto);

//            qDebug() << "Current timestamp:" << d->m_position.toString("hh:mm:ss.zzz");

        if (d->m_currentFrame) {
            d->m_position = OpenGLRendererPrivate::timeFromMillis(d->m_currentFrame->pts / 1000);
        }

        QString overlay(d->m_position.toString("hh:mm:ss.zzz") + "/" + d->m_duration.toString("hh:mm:ss.zzz"));
        QFontMetrics fm(roboto);
        QRect overlayRect = fm.boundingRect(overlay);
        overlayRect.moveTopLeft({20, 20});
        p.fillRect(overlayRect.adjusted(-5, 0, 5, 0), QColor(0xFF, 0xFF, 0xFF, 0x48));
        p.drawText(overlayRect, overlay);
        p.end();
        qDebug() << "Paused:" << (d->m_paused.load() ? "true" : "false");
        if (!d->m_paused.load()) {
            update();
        }
    }

#pragma clang diagnostic pop

    void OpenGLRenderer::mouseReleaseEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            pause(nullptr, !isPaused());
        }
    }

    void OpenGLRenderer::closeEvent(QCloseEvent *event) {
        QApplication::quit();
        QWidget::closeEvent(event);
    }

    RenderClock *OpenGLRenderer::getClock() {
        Q_D(AVQt::OpenGLRenderer);
        return d->m_clock;
    }

    [[maybe_unused]] GLint
    OpenGLRendererPrivate::project(GLdouble objx, GLdouble objy, GLdouble objz, const GLdouble model[16], const GLdouble proj[16],
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
    }

    void OpenGLRendererPrivate::transformPoint(GLdouble *out, const GLdouble *m, const GLdouble *in) {
#define M(row, col)  m[(col) * 4 + (row)]
        out[0] = M(0, 0) * in[0] + M(0, 1) * in[1] + M(0, 2) * in[2] + M(0, 3) * in[3];
        out[1] = M(1, 0) * in[0] + M(1, 1) * in[1] + M(1, 2) * in[2] + M(1, 3) * in[3];
        out[2] = M(2, 0) * in[0] + M(2, 1) * in[1] + M(2, 2) * in[2] + M(2, 3) * in[3];
        out[3] = M(3, 0) * in[0] + M(3, 1) * in[1] + M(3, 2) * in[2] + M(3, 3) * in[3];
#undef M
    }

    QTime OpenGLRendererPrivate::timeFromMillis(int64_t ts) {
        int ms = static_cast<int>(ts % 1000);
        int s = static_cast<int>((ts / 1000) % 60);
        int m = static_cast<int>((ts / 1000 / 60) % 60);
        int h = static_cast<int>(ts / 1000 / 60 / 60);
        return QTime(h, m, s, ms);
    }
}