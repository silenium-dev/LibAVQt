#include "renderers/private/OpenGLRenderer_p.h"

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
    std::atomic_bool OpenGLRendererPrivate::resourcesLoaded{false};
    OpenGLRendererPrivate::OpenGLRendererPrivate(OpenGLRenderer *q) : q_ptr(q) {
    }

    void OpenGLRendererPrivate::initializeGL(QOpenGLContext *context) {
        Q_Q(AVQt::OpenGLRenderer);
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

        m_program = new QOpenGLShaderProgram();
        m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
        m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);

        m_program->bindAttributeLocation("vertex", OpenGLRendererPrivate::PROGRAM_VERTEX_ATTRIBUTE);
        m_program->bindAttributeLocation("texCoord", OpenGLRendererPrivate::PROGRAM_TEXCOORD_ATTRIBUTE);

        if (!m_program->link()) {
            qDebug() << "Shader linkers errors:\n"
                     << m_program->log();
        }

        m_program->bind();
        m_program->setUniformValue("textureY", 0);
        m_program->setUniformValue("textureU", 1);
        m_program->setUniformValue("textureV", 2);
        m_program->release();
    }// Generic

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
            case AV_PIX_FMT_BGRA:
                m_program->setUniformValue("inputFormat", 0);
                break;
            case AV_PIX_FMT_VAAPI:
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
    }// Platform

    void OpenGLRendererPrivate::onFrame(AVFrame *frame, AVBufferRef *pDeviceCtx) {
        QMutexLocker onFrameLock{&m_onFrameMutex};

        QFuture<AVFrame *> queueFrame;

        constexpr auto strBufSize = 64;
        char strBuf[strBufSize];
        qDebug("onFrame() of OpenGLRenderer");
        qDebug("Pixel format: %s", av_get_pix_fmt_string(strBuf, 64, static_cast<AVPixelFormat>(frame->format)));
        switch (frame->format) {
            case AV_PIX_FMT_QSV:
            case AV_PIX_FMT_CUDA:
            case AV_PIX_FMT_VDPAU:
                qDebug("Transferring frame from GPU to CPU");
                queueFrame =
                        QtConcurrent::run([](AVFrame *input) {
                            AVFrame *outFrame = av_frame_alloc();
                            int ret = av_hwframe_transfer_data(outFrame, input, 0);
                            if (ret != 0) {
                                char strBuf[strBufSize];
                                qFatal("[AVQt::OpenGLRenderer] %i: Could not transfer frame from GPU to CPU: %s", ret,
                                       av_make_error_string(strBuf, strBufSize, ret));
                            }
                            outFrame->pts = input->pts;
                            av_frame_free(&input);
                            return outFrame;
                        },
                                          av_frame_clone(frame));
                break;
            default:
                qDebug("Referencing frame");
                queueFrame =
                        QtConcurrent::run([this](AVFrame *input, AVBufferRef *pDeviceCtx) {
                            bool shouldBe = true;
                            if (m_firstFrame.compare_exchange_strong(shouldBe, false) && input->format == AV_PIX_FMT_VAAPI) {
                                m_pVAContext = static_cast<AVVAAPIDeviceContext *>(reinterpret_cast<AVHWDeviceContext *>(pDeviceCtx->data)->hwctx);
                                m_VADisplay = m_pVAContext->display;
                            }
                            return input;
                        },
                                          av_frame_clone(frame), pDeviceCtx);
                break;
        }

        while (m_renderQueue.size() > 8) {
            QThread::msleep(4);
        }

        QMutexLocker lock(&m_renderQueueMutex);
        m_renderQueue.enqueue(queueFrame);
    }// Platform

    void OpenGLRendererPrivate::initializePlatformAPI() {
        EGLint visual_attr[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                EGL_NONE};
        m_EGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        //        if (m_EGLDisplay == EGL_NO_DISPLAY) {
        //            qDebug("Could not get default EGL display, connecting to X-Server");
        //            Display *display = XOpenDisplay(nullptr);
        //            if (!display) {
        //                qFatal("Could not get X11 display");
        //            }
        //            m_EGLDisplay = eglGetDisplay(static_cast<EGLNativeDisplayType>(display));
        if (m_EGLDisplay == EGL_NO_DISPLAY) {
            qFatal("Could not get EGL display: %s", eglErrorString(eglGetError()).c_str());
        }
        //        }
        if (!eglInitialize(m_EGLDisplay, nullptr, nullptr)) {
            qFatal("eglInitialize");
        }
        if (!eglBindAPI(EGL_OPENGL_API)) {
            qFatal("eglBindAPI");
        }

        EGLConfig cfg;
        EGLint cfg_count;
        if (!eglChooseConfig(m_EGLDisplay, visual_attr, &cfg, 1, &cfg_count) || (cfg_count < 1)) {
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
        //        m_EGLContext = eglCreateContext(m_EGLDisplay, cfg, EGL_NO_CONTEXT, ctx_attr);
        //        if (m_EGLContext == EGL_NO_CONTEXT) {
        //            qFatal("eglCreateContext");
        //        }

        qDebug("EGL Version: %s", eglQueryString(m_EGLDisplay, EGL_VERSION));
    }// Platform

    void OpenGLRendererPrivate::initializeOnFirstFrame() {
        // Frame has 64 pixel alignment, set max height coord to cut off additional pixels
        float maxTexHeight = 1.0f;

        if (m_currentFrame->format == AV_PIX_FMT_VAAPI) {
            VASurfaceID vaSurfaceId = reinterpret_cast<uintptr_t>(m_currentFrame->data[3]);
            VAImage vaImage;
            vaDeriveImage(m_VADisplay, vaSurfaceId, &vaImage);
            maxTexHeight = static_cast<float>(m_currentFrame->height * 1.0 / (vaImage.height + 2.0));
            vaDestroyImage(m_VADisplay, vaImage.image_id);
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

        m_vbo = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        m_vbo.create();
        m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_vbo.bind();

        m_vbo.allocate(vertexBufferData.data(), static_cast<int>(vertexBufferData.size() * sizeof(float)));

        m_vao.create();
        m_vao.bind();

        uint indices[] = {
                0, 1, 3,// first tri
                1, 2, 3 // second tri
        };

        m_ibo = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
        m_ibo.create();
        m_ibo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_ibo.bind();
        m_ibo.allocate(indices, sizeof(indices));

        int stride = 5 * sizeof(float);

        // layout location 0 - vec3 with coords
        m_program->enableAttributeArray(0);
        m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);

        // layout location 1 - vec2 with texture coordinates
        m_program->enableAttributeArray(1);
        int texCoordsOffset = 3 * sizeof(float);
        m_program->setAttributeBuffer(1, GL_FLOAT, texCoordsOffset, 2, stride);

        // Release (unbind) all
        m_vbo.release();
        m_vao.release();

        if (m_currentFrame->format == AV_PIX_FMT_VAAPI) {
            glGenTextures(2, m_textures);
            for (const auto &texture : m_textures) {
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            m_program->bind();
            m_program->setUniformValue("inputFormat", 1);
            m_program->release();
        } else {
            bool VTexActive = false, UTexActive = false;
            QSize YSize, USize{-1, -1}, VSize{-1, -1};
            QOpenGLTexture::TextureFormat textureFormatY, textureFormatU{}, textureFormatV{};
            QOpenGLTexture::PixelFormat pixelFormatY, pixelFormatU{}, pixelFormatV{};
            QOpenGLTexture::PixelType pixelType;
            switch (static_cast<AVPixelFormat>(m_currentFrame->format)) {
                case AV_PIX_FMT_BGRA:
                    YSize = QSize(m_currentFrame->width, m_currentFrame->height);
                    textureFormatY = QOpenGLTexture::RGBA8_UNorm;
                    pixelFormatY = QOpenGLTexture::BGRA;
                    pixelType = QOpenGLTexture::UInt8;
                    break;
                case AV_PIX_FMT_YUV420P:
                    YSize = QSize(m_currentFrame->width, m_currentFrame->height);
                    USize = VSize = QSize(m_currentFrame->width / 2, m_currentFrame->height / 2);
                    textureFormatY = textureFormatU = textureFormatV = QOpenGLTexture::R8_UNorm;
                    pixelFormatY = pixelFormatU = pixelFormatV = QOpenGLTexture::Red;
                    pixelType = QOpenGLTexture::UInt8;
                    UTexActive = VTexActive = true;
                    break;
                case AV_PIX_FMT_YUV420P10:
                    YSize = QSize(m_currentFrame->width, m_currentFrame->height);
                    USize = VSize = QSize(m_currentFrame->width / 2, m_currentFrame->height / 2);
                    textureFormatY = textureFormatU = textureFormatV = QOpenGLTexture::R16_UNorm;
                    pixelFormatY = pixelFormatU = pixelFormatV = QOpenGLTexture::Red;
                    pixelType = QOpenGLTexture::UInt16;
                    UTexActive = VTexActive = true;
                    break;
                case AV_PIX_FMT_NV12:
                    YSize = QSize(m_currentFrame->width, m_currentFrame->height);
                    USize = QSize(m_currentFrame->width / 2, m_currentFrame->height / 2);
                    textureFormatY = QOpenGLTexture::R8_UNorm;
                    textureFormatU = QOpenGLTexture::RG8_UNorm;
                    pixelFormatY = QOpenGLTexture::Red;
                    pixelFormatU = QOpenGLTexture::RG;
                    pixelType = QOpenGLTexture::UInt8;
                    UTexActive = true;
                    break;
                case AV_PIX_FMT_P010:
                    YSize = QSize(m_currentFrame->width, m_currentFrame->height);
                    USize = QSize(m_currentFrame->width / 2, m_currentFrame->height / 2);
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
            m_yTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
            m_yTexture->setSize(YSize.width(), YSize.height());
            m_yTexture->setFormat(textureFormatY);
            m_yTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
            m_yTexture->allocateStorage(pixelFormatY, pixelType);
            if (UTexActive) {
                m_uTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
                m_uTexture->setSize(USize.width(), USize.height());
                m_uTexture->setFormat(textureFormatU);
                m_uTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                m_uTexture->allocateStorage(pixelFormatU, pixelType);
            }
            if (VTexActive) {
                m_vTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
                m_vTexture->setSize(VSize.width(), VSize.height());
                m_vTexture->setFormat(textureFormatV);
                m_vTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                m_vTexture->allocateStorage(pixelFormatV, pixelType);
            }
        }
    }// Platform

    void OpenGLRendererPrivate::mapFrame() {
        LOOKUP_FUNCTION(PFNEGLCREATEIMAGEKHRPROC, eglCreateImageKHR)
        LOOKUP_FUNCTION(PFNEGLDESTROYIMAGEKHRPROC, eglDestroyImageKHR)
        LOOKUP_FUNCTION(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC, glEGLImageTargetTexture2DOES)

        if (m_currentFrame->format == AV_PIX_FMT_VAAPI) {
            for (int i = 0; i < 2; ++i) {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, m_textures[i]);
                eglDestroyImageKHR(m_EGLDisplay, m_EGLImages[i]);
            }

            VASurfaceID va_surface = reinterpret_cast<uintptr_t>(m_currentFrame->data[3]);

            //                        VAImage vaImage;
            //                        vaDeriveImage(m_VADisplay, va_surface, &vaImage);
            //                        VABufferInfo vaBufferInfo;
            //                        memset(&vaBufferInfo, 0, sizeof(VABufferInfo));
            //                        vaBufferInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_VA;
            //                        vaAcquireBufferHandle(m_VADisplay, vaImage.buf, &vaBufferInfo);

            VADRMPRIMESurfaceDescriptor prime;
            if (vaExportSurfaceHandle(m_VADisplay, va_surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                      VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS, &prime) !=
                VA_STATUS_SUCCESS) {
                qFatal("[AVQt::OpenGLRenderer] Could not export VA surface handle");
            }
            vaSyncSurface(m_VADisplay, va_surface);

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
                m_EGLImages[i] = eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                                   img_attr);

                auto error = eglGetError();
                if (!m_EGLImages[i] || m_EGLImages[i] == EGL_NO_IMAGE_KHR || error != EGL_SUCCESS) {
                    qFatal("[AVQt::OpenGLRenderer] Could not create %s EGLImage: %s", (i ? "UV" : "Y"),
                           eglErrorString(error).c_str());
                }
#undef LAYER
#undef PLANE

                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, m_textures[i]);
                while (glGetError() != GL_NO_ERROR)
                    ;
                glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_EGLImages[i]);
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
            //                        vaReleaseBufferHandle(m_VADisplay, vaImage.buf);
            //                        vaDestroyImage(m_VADisplay, vaImage.image_id);
            for (int i = 0; i < (int) prime.num_objects; ++i) {
                ::close(prime.objects[i].fd);
            }
        } else {
            m_yTexture->bind(0);
            if (m_uTexture) {
                m_uTexture->bind(1);
            }
            if (m_vTexture) {
                m_vTexture->bind(2);
            }
            switch (m_currentFrame->format) {
                case AV_PIX_FMT_BGRA:
                    m_yTexture->setData(QOpenGLTexture::PixelFormat::BGRA, QOpenGLTexture::UInt8,
                                        const_cast<const uint8_t *>(m_currentFrame->data[0]));
                    break;
                case AV_PIX_FMT_NV12:
                    m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                                        const_cast<const uint8_t *>(m_currentFrame->data[0]));
                    m_uTexture->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt8,
                                        const_cast<const uint8_t *>(m_currentFrame->data[1]));
                    break;
                case AV_PIX_FMT_P010:
                    m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16,
                                        const_cast<const uint8_t *>(m_currentFrame->data[0]));
                    m_uTexture->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt16,
                                        const_cast<const uint8_t *>(m_currentFrame->data[1]));
                    break;
                case AV_PIX_FMT_YUV420P:
                    m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                                        const_cast<const uint8_t *>(m_currentFrame->data[0]));
                    m_uTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                                        const_cast<const uint8_t *>(m_currentFrame->data[1]));
                    m_vTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                                        const_cast<const uint8_t *>(m_currentFrame->data[2]));
                    break;
                case AV_PIX_FMT_YUV420P10:
                    m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16,
                                        const_cast<const uint8_t *>(m_currentFrame->data[0]));
                    m_uTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16,
                                        const_cast<const uint8_t *>(m_currentFrame->data[1]));
                    m_vTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16,
                                        const_cast<const uint8_t *>(m_currentFrame->data[2]));
                    break;
                default:
                    qFatal("Pixel format not supported");
            }
        }
    }// Platform

    void OpenGLRendererPrivate::bindResources() {
        m_program->bind();
        if (m_currentFrame->format == AV_PIX_FMT_VAAPI) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_textures[0]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_textures[1]);
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
        if (m_context) {
            if (m_context->isValid() && m_context->makeCurrent(m_surface)) {
                delete m_program;
                m_program = nullptr;

                m_ibo.destroy();
                m_vbo.destroy();
                m_vao.destroy();

                delete m_yTexture;
                delete m_uTexture;
                delete m_vTexture;

                m_yTexture = nullptr;
                m_uTexture = nullptr;
                m_vTexture = nullptr;
                m_context->doneCurrent();
            }
        }

        if (m_EGLImages[0]) {
            for (auto &EGLImage : m_EGLImages) {
                if (EGLImage != nullptr) {
                    eglDestroyImage(m_EGLDisplay, EGLImage);
                }
            }
        }
    }// Platform
}// namespace AVQt
