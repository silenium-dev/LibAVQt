//
// Created by silas on 3/21/21.
//

#include "private/OpenGLRenderer_p.h"
#include "OpenGLRenderer.h"

#ifdef Q_OS_LINUX

#include <va/va.h>
#include <va/va_drmcommon.h>
#include <libdrm/drm_fourcc.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GL/glu.h>
#elif defined(Q_OS_WINDOWS)

#include <comdef.h>
#include <d3d9.h>

#define WGL_LOOKUP_FUNCTION(type, func)         \
    type func = (type)wglGetProcAddress(#func); \
    if (!(func))                                \
    {                                           \
        qFatal("wglGetProcAddress(" #func ")"); \
    }

typedef HANDLE(WINAPI *PFNWGLDXOPENDEVICENVPROC)(void *dxDevice);

typedef BOOL(WINAPI *PFNWGLDXCLOSEDEVICENVPROC)(HANDLE hDevice);

typedef BOOL(WINAPI *PFNWGLDXSETRESOURCESHAREHANDLENVPROC)(void *dxObject, HANDLE shareHandle);

typedef HANDLE(WINAPI *PFNWGLDXREGISTEROBJECTNVPROC)(HANDLE hDevice, void *dxObject, GLuint name, GLenum type, GLenum access);

typedef BOOL(WINAPI *PFNWGLDXUNREGISTEROBJECTNVPROC)(HANDLE hDevice, HANDLE hObject);

typedef BOOL(WINAPI *PFNWGLDXLOCKOBJECTSNVPROC)(HANDLE hDevice, GLint count, HANDLE *hObjects);

typedef BOOL(WINAPI *PFNWGLDXUNLOCKOBJECTSNVPROC)(HANDLE hDevice, GLint count, HANDLE *hObjects);

typedef BOOL(WINAPI *PFNWGLDXOBJECTACCESSNVPROC)(HANDLE hObject, GLenum access);

#define WGL_ACCESS_READ_ONLY_NV 0x0000
#define WGL_ACCESS_READ_WRITE_NV 0x0001
#define WGL_ACCESS_WRITE_DISCARD_NV 0x0002

#undef ASSERT
#define ASSERT(expr)                              \
    if (!expr)                                    \
    {                                             \
        qFatal("Assertion \"" #expr "\" failed"); \
    }

std::string GetLastErrorAsString()
{
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
    {
        return {}; //No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 nullptr, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

[[maybe_unused]] QImage SurfaceToQImage(IDirect3DSurface9 *pSurface /*, IDirect3DDevice9 *pDevice*/, int height)
{
    D3DSURFACE_DESC surfaceDesc;
    pSurface->GetDesc(&surfaceDesc);
    //    IDirect3DSurface9 *pRGBASurface;
    //    HANDLE hRGBASurface;
    //    ASSERT(SUCCEEDED(
    //            pDevice->CreateOffscreenPlainSurface(surfaceDesc.Width, surfaceDesc.Height, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pRGBASurface,
    //                                                 &hRGBASurface)));
    //    ASSERT(SUCCEEDED(pDevice->StretchRect(pSurface, nullptr, pRGBASurface, nullptr, D3DTEXF_LINEAR)));
    //    BYTE *dataPtr = reinterpret_cast<BYTE *>(locked.pBits);

    //    if (locked.Pitch == 0) {
    //        return {};
    //    }
    //    QThread::sleep(1);

    QImage::Format imageFormat;
    uint bpp;
    switch (surfaceDesc.Format)
    {
        //        case D3DFMT_A16B16G16R16:
        //            imageFormat = QImage::Format_RGBA64;
        //            bpp = 64;
        //            break;
        //        case D3DFMT_A8R8G8B8:
        //            imageFormat = QImage::Format_ARGB32;
        //            bpp = 32;
        //            break;
    default:
        imageFormat = QImage::Format_Grayscale8;
        bpp = 8;
        break;
    }
    bpp /= 8;

    QImage result(surfaceDesc.Width, surfaceDesc.Height, imageFormat);

    D3DLOCKED_RECT locked;
    ASSERT(SUCCEEDED(pSurface->LockRect(&locked, nullptr, D3DLOCK_READONLY)));
    qDebug("Locked pitch: %d", locked.Pitch);
    //    locked.Pitch = 2048;
    qDebug() << locked.pBits << locked.Pitch << surfaceDesc.Width;
    for (uint y = 0; y < surfaceDesc.Height; ++y)
    {
        qDebug() << y << ": From" << ((uint8_t *)locked.pBits) + y * locked.Pitch << ", To:"
                 << result.scanLine(y) << ", Size:" << surfaceDesc.Width * bpp;
        memcpy(result.scanLine(y), ((uint8_t *)locked.pBits) + y * locked.Pitch,
               surfaceDesc.Width * bpp);
    }

    //    for (uint i = 0; i < 10; ++i) {
    //        qDebug() << i << ":" << locked.pBits << locked.Pitch << surfaceDesc.Width;
    //        QThread::msleep(20);
    //    }

    //    QImage result = QImage((uint8_t*)locked.pBits, locked.Pitch, surfaceDesc.Height, locked.Pitch, QImage::Format_Grayscale8);
    //    QFile out("output2.bmp");
    //    out.open(QIODevice::Truncate | QIODevice::ReadWrite);
    //    result.save(&out, "BMP");
    //    out.flush();
    //    out.close();
    //    qDebug() << result.size() << "using" << result.sizeInBytes() << "Byte";

    //    delete[] data;
    ASSERT(SUCCEEDED(pSurface->UnlockRect()));

    return result;
}

#endif

#include <iostream>

extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>

#ifdef Q_OS_LINUX
#include <libavutil/hwcontext_vaapi.h>
//#elif defined(Q_OS_WINDOWS)
//#include <libavutil/hwcontext_dxva2.h>
#endif
}

#define LOOKUP_FUNCTION(type, func)             \
    type func = (type)eglGetProcAddress(#func); \
    if (!(func))                                \
    {                                           \
        qFatal("eglGetProcAddress(" #func ")"); \
    }

static void loadResources()
{
    Q_INIT_RESOURCE(resources);
}

#ifdef Q_OS_LINUX
static int closefd(int fd)
{
    return close(fd);
}
#endif

#include <QtGui>
#include <QtConcurrent>

std::string eglErrorString(EGLint error)
{
    switch (error)
    {
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

namespace AVQt
{
    OpenGLRenderer::OpenGLRenderer(QWidget *parent) : QOpenGLWidget(parent),
                                                      d_ptr(new OpenGLRendererPrivate(this))
    {
        setAttribute(Qt::WA_QuitOnClose);
    }

    [[maybe_unused]] [[maybe_unused]] OpenGLRenderer::OpenGLRenderer(OpenGLRendererPrivate &p) : d_ptr(&p)
    {
    }

    OpenGLRenderer::OpenGLRenderer(OpenGLRenderer &&other) noexcept : d_ptr(other.d_ptr)
    {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    OpenGLRenderer::~OpenGLRenderer() noexcept
    {
        delete d_ptr;
    }

    int OpenGLRenderer::init(IFrameSource *source, AVRational framerate, int64_t duration)
    {
        Q_UNUSED(framerate) // Don't use framerate, because it is not always specified in stream information
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        d->m_duration = OpenGLRendererPrivate::timeFromMillis(duration);
        qDebug() << "Duration:" << d->m_duration.toString("hh:mm:ss.zzz");

        d->m_clock = new RenderClock;
        d->m_clock->setInterval(16);

        return 0;
    }

    int OpenGLRenderer::deinit(IFrameSource *source)
    {
        Q_D(AVQt::OpenGLRenderer);
        stop(source);

        if (d->m_pQSVDerivedDeviceContext)
        {
            av_buffer_unref(&d->m_pQSVDerivedFramesContext);
            av_buffer_unref(&d->m_pQSVDerivedDeviceContext);
        }

        delete d->m_clock;

        return 0;
    }

    int OpenGLRenderer::start(IFrameSource *source)
    {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        bool stopped = false;
        if (d->m_running.compare_exchange_strong(stopped, true))
        {
            d->m_paused.store(false);
            qDebug("Started renderer");

            QMetaObject::invokeMethod(this, "showNormal", Qt::QueuedConnection);
            //            QMetaObject::invokeMethod(this, "requestActivate", Qt::QueuedConnection);
            QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);

            started();
        }
        return 0;
    }

    int OpenGLRenderer::stop(IFrameSource *source)
    {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        bool shouldBeCurrent = true;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, false))
        {
            hide();

            if (d->m_currentFrame)
            {
                av_frame_free(&d->m_currentFrame);
            }

            if (d->m_clock)
            {
                d->m_clock->stop();
            }

            {
                QMutexLocker lock(&d->m_renderQueueMutex);

                for (auto &e : d->m_renderQueue)
                {
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

#ifdef Q_OS_LINUX
            if (d->m_EGLImages[0])
            {
                for (auto &EGLImage : d->m_EGLImages)
                {
                    eglDestroyImage(d->m_EGLDisplay, EGLImage);
                }
            }
#endif

            stopped();
            return 0;
        }
        return -1;
    }

    void OpenGLRenderer::pause(IFrameSource *source, bool pause)
    {
        Q_UNUSED(source)
        Q_D(AVQt::OpenGLRenderer);

        bool shouldBeCurrent = !pause;

        if (d->m_paused.compare_exchange_strong(shouldBeCurrent, pause))
        {
            if (d->m_clock->isActive())
            {
                d->m_clock->pause(pause);
            }
            qDebug("pause() called");
            paused(pause);
            update();
        }
    }

    bool OpenGLRenderer::isPaused()
    {
        Q_D(AVQt::OpenGLRenderer);

        return d->m_paused.load();
    }

    void OpenGLRenderer::onFrame(IFrameSource *source, AVFrame *frame, int64_t duration, AVBufferRef *pDeviceCtx)
    {
        Q_D(AVQt::OpenGLRenderer);
        Q_UNUSED(source)
        Q_UNUSED(duration)

        QMutexLocker onFrameLock{&d->m_onFrameMutex};

        QFuture<AVFrame *> queueFrame;

        //        av_frame_ref(newFrame.first, frame);
        constexpr auto strBufSize = 64;
        char strBuf[strBufSize];
        qDebug("Pixel format: %s", av_get_pix_fmt_string(strBuf, 64, static_cast<AVPixelFormat>(frame->format)));
        switch (frame->format)
        {
        case AV_PIX_FMT_QSV:
        case AV_PIX_FMT_CUDA:
        case AV_PIX_FMT_VDPAU:
            //        case AV_PIX_FMT_DXVA2_VLD:
            qDebug("Transferring frame from GPU to CPU");
            queueFrame = QtConcurrent::run([](AVFrame *input)
                                           {
                                               AVFrame *outFrame = av_frame_alloc();
                                               int ret = av_hwframe_transfer_data(outFrame, input, 0);
                                               if (ret != 0)
                                               {
                                                   char strBuf[strBufSize];
                                                   qFatal("[AVQt::OpenGLRenderer] %i: Could not transfer frame from GPU to CPU: %s", ret,
                                                          av_make_error_string(strBuf, strBufSize, ret));
                                               }
                                               //                    outFrame->pts = input->pts;
                                               av_frame_free(&input);
                                               return outFrame;
                                           },
                                           av_frame_clone(frame));
            break;
            //            case AV_PIX_FMT_QSV: {
            //                qDebug("[AVQt::OpenGLRenderer] Mapping QSV frame to CPU for rendering");
            //                queueFrame = QtConcurrent::run([d](AVFrame *input) {
            //                    AVFrame *outFrame = av_frame_alloc();
            //                    int ret = av_hwframe_map(outFrame, input, AV_HWFRAME_MAP_READ);
            //                    if (ret != 0) {
            //                        constexpr auto strBufSize = 64;
            //                        char strBuf[strBufSize];
            //                        qFatal("[AVQt::OpenGLRenderer] %d Could not map QSV frame to CPU: %s", ret,
            //                               av_make_error_string(strBuf, strBufSize, ret));
            //                    }
            //                    outFrame->pts = input->pts;
            //                    av_frame_free(&input);
            //                    return outFrame;
            //                }, av_frame_clone(frame));
            //                break;
            //            }
        default:
            qDebug("Referencing frame");
            queueFrame = QtConcurrent::run([d](AVFrame *input, AVBufferRef *pDeviceCtx)
                                           {
#ifdef Q_OS_LINUX
                                               bool shouldBe = true;
                                               if (d->m_firstFrame.compare_exchange_strong(shouldBe, false) && input->format == AV_PIX_FMT_VAAPI)
                                               {
                                                   d->m_pVAContext = static_cast<AVVAAPIDeviceContext *>(reinterpret_cast<AVHWDeviceContext *>(pDeviceCtx->data)->hwctx);
                                                   d->m_VADisplay = d->m_pVAContext->display;
                                               }
#elif defined(Q_OS_WINDOWS)
                                               bool shouldBe = true;
                                               if (d->m_firstFrame.compare_exchange_strong(shouldBe, false))
                                               {
                                                   if (input->format == AV_PIX_FMT_DXVA2_VLD)
                                                   {
                                                       //                            AVFrame *sw_frame = av_frame_alloc();
                                                       //                            if (av_hwframe_transfer_data(sw_frame, input, 0)) {
                                                       //                                qFatal("Could not map frame");
                                                       //                            }
                                                       //                            QImage result(sw_frame->data[0], sw_frame->width, sw_frame->height, sw_frame->linesize[0], QImage::Format_Grayscale8);
                                                       //                            qDebug() << "Dimensions:" << result.size() << ", Size:" << result.sizeInBytes();
                                                       //                            result.save("output3.bmp");
                                                       //                            exit(0);
                                                       d->m_pDXVAContext = static_cast<AVDXVA2DeviceContext *>(reinterpret_cast<AVHWDeviceContext *>(pDeviceCtx->data)->hwctx);
                                                       d->m_pD3DManager = d->m_pDXVAContext->devmgr;
                                                       d->m_pD3DManager->AddRef();
                                                       //                            HANDLE hDevice;
                                                       //                            IDirect3DDevice9 *pDevice;
                                                       //                            d->m_pD3DManager->OpenDeviceHandle(&hDevice);
                                                       //                            d->m_pD3DManager->LockDevice(hDevice, &pDevice, TRUE);
                                                       //                            SurfaceToQImage(reinterpret_cast<LPDIRECT3DSURFACE9>(input->data[3])/*, pDevice*/, input->height);
                                                       //                            d->m_pD3DManager->UnlockDevice(hDevice, TRUE);
                                                       //                            d->m_pD3DManager->CloseDeviceHandle(hDevice);
                                                   }
                                                   else if (input->format == AV_PIX_FMT_D3D11)
                                                   {
                                                       d->m_pD3D11VAContext = static_cast<AVD3D11VADeviceContext *>(reinterpret_cast<AVHWDeviceContext *>(pDeviceCtx->data)->hwctx);
                                                   }
                                               }
#endif
                                               return input;
                                           },
                                           av_frame_clone(frame), pDeviceCtx);
            break;
        }

        //        av_frame_unref(frame);

        //        char strBuf[64];
        //qDebug() << "Pixel format:" << av_get_pix_fmt_string(strBuf, 64, static_cast<AVPixelFormat>(frame->format));

        while (d->m_renderQueue.size() > 6)
        {
            QThread::msleep(4);
        }

        QMutexLocker lock(&d->m_renderQueueMutex);
        d->m_renderQueue.enqueue(queueFrame);
    }

    void OpenGLRenderer::initializeGL()
    {
        Q_D(AVQt::OpenGLRenderer);

        loadResources();

#ifdef Q_OS_LINUX
        EGLint visual_attr[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_NONE};
        d->m_EGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        //        if (d->m_EGLDisplay == EGL_NO_DISPLAY) {
        //            qDebug("Could not get default EGL display, connecting to X-Server");
        //            Display *display = XOpenDisplay(nullptr);
        //            if (!display) {
        //                qFatal("Could not get X11 display");
        //            }
        //            d->m_EGLDisplay = eglGetDisplay(static_cast<EGLNativeDisplayType>(display));
        if (d->m_EGLDisplay == EGL_NO_DISPLAY)
        {
            qFatal("Could not get EGL display: %s", eglErrorString(eglGetError()).c_str());
        }
        //        }
        if (!eglInitialize(d->m_EGLDisplay, nullptr, nullptr))
        {
            qFatal("eglInitialize");
        }
        if (!eglBindAPI(EGL_OPENGL_API))
        {
            qFatal("eglBindAPI");
        }

        EGLConfig cfg;
        EGLint cfg_count;
        if (!eglChooseConfig(d->m_EGLDisplay, visual_attr, &cfg, 1, &cfg_count) || (cfg_count < 1))
        {
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
#endif

        QByteArray shaderVersionString;

        if (context()->isOpenGLES())
        {
            shaderVersionString = "#version 300 es\n";
        }
        else
        {
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

        if (!d->m_program->link())
        {
            qDebug() << "Shader linkers errors:\n"
                     << d->m_program->log();
        }

        d->m_program->bind();
        d->m_program->setUniformValue("textureY", 0);
        d->m_program->setUniformValue("textureU", 1);
        d->m_program->setUniformValue("textureV", 2);
        d->m_program->release();
    }

    void OpenGLRenderer::paintGL()
    {
        Q_D(AVQt::OpenGLRenderer);
        //        auto t1 = std::chrono::high_resolution_clock::now();

#ifdef Q_OS_LINUX
        LOOKUP_FUNCTION(PFNEGLCREATEIMAGEKHRPROC, eglCreateImageKHR)
        LOOKUP_FUNCTION(PFNEGLDESTROYIMAGEKHRPROC, eglDestroyImageKHR)
        LOOKUP_FUNCTION(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC, glEGLImageTargetTexture2DOES)
#elif defined(Q_OS_WINDOWS)
        WGL_LOOKUP_FUNCTION(PFNWGLDXOPENDEVICENVPROC, wglDXOpenDeviceNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXCLOSEDEVICENVPROC, wglDXCloseDeviceNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXREGISTEROBJECTNVPROC, wglDXRegisterObjectNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXUNREGISTEROBJECTNVPROC, wglDXUnregisterObjectNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXLOCKOBJECTSNVPROC, wglDXLockObjectsNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXUNLOCKOBJECTSNVPROC, wglDXUnlockObjectsNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXOBJECTACCESSNVPROC, wglDXObjectAccessNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXSETRESOURCESHAREHANDLENVPROC, wglDXSetResourceShareHandleNV)
        WGL_LOOKUP_FUNCTION(PFNGLACTIVETEXTUREPROC, glActiveTexture)
#endif

        if (d->m_currentFrame)
        {
            int display_width = width();
            int display_height = (width() * d->m_currentFrame->height + d->m_currentFrame->width / 2) / d->m_currentFrame->width;
            if (display_height > height())
            {
                display_width = (height() * d->m_currentFrame->width + d->m_currentFrame->height / 2) / d->m_currentFrame->height;
                display_height = height();
            }
            glViewport((width() - display_width) / 2, (height() - display_height) / 2, display_width, display_height);
        }

        //         Clear background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        //        qDebug("paintGL() – Running: %d", d->m_running.load());

        if (d->m_running.load())
        {
            if (!d->m_paused.load())
            {
                if (d->m_clock)
                {
                    if (!d->m_clock->isActive() && d->m_renderQueue.size() > 2)
                    {
                        d->m_clock->start();
                    }
                    else if (d->m_clock->isPaused())
                    {
                        d->m_clock->pause(false);
                    }
                }
                if (!d->m_renderQueue.isEmpty())
                {
                    auto timestamp = d->m_clock->getTimestamp();
                    if (timestamp >= d->m_renderQueue.first().result()->pts)
                    {
                        d->m_updateRequired.store(true);
                        d->m_updateTimestamp.store(timestamp);
                    }
                }
                if (d->m_updateRequired.load() && !d->m_renderQueue.isEmpty())
                {
                    d->m_updateRequired.store(false);
                    auto frame = d->m_renderQueue.dequeue().result();
                    while (!d->m_renderQueue.isEmpty())
                    {
                        if (frame->pts <= d->m_updateTimestamp.load())
                        {
                            if (d->m_renderQueue.first().result()->pts >= d->m_updateTimestamp.load())
                            {
                                break;
                            }
                            else
                            {
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
                        if (!firstFrame)
                        {
                            if (d->m_currentFrame->format == frame->format)
                            {
                                differentPixFmt = false;
                            }
                        }

                        if (d->m_currentFrame)
                        {
                            av_frame_free(&d->m_currentFrame);
                        }

                        d->m_currentFrame = frame;
                    }

                    if (firstFrame)
                    {
                        // Frame has 64 pixel alignment, set max height coord to cut off additional pixels
                        float maxTexHeight = 1.0f;
#ifdef Q_OS_LINUX
                        if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI)
                        {
                            VASurfaceID vaSurfaceId = reinterpret_cast<uintptr_t>(d->m_currentFrame->data[3]);
                            VAImage vaImage;
                            vaDeriveImage(d->m_VADisplay, vaSurfaceId, &vaImage);
                            maxTexHeight = static_cast<float>(d->m_currentFrame->height * 1.0 / (vaImage.height + 2.0));
                            vaDestroyImage(d->m_VADisplay, vaImage.image_id);
                        }
#elif defined(Q_OS_WINDOWS)
                        if (d->m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD)
                        {
                            LPDIRECT3DSURFACE9 surface = (LPDIRECT3DSURFACE9)d->m_currentFrame->data[3];
                            D3DSURFACE_DESC surfaceDesc;
                            surface->GetDesc(&surfaceDesc);
                            maxTexHeight = static_cast<float>(d->m_currentFrame->height * 1.0 / surfaceDesc.Height);
                        }
                        else if (d->m_currentFrame->format == AV_PIX_FMT_D3D11)
                        {
                        }
#endif

                        float vertices[] = {
                            1, 1, 0,   // top right
                            1, -1, 0,  // bottom right
                            -1, -1, 0, // bottom left
                            -1, 1, 0   // top left
                        };

                        float vertTexCoords[] = {
                            0, 0, maxTexHeight, maxTexHeight,
                            0, 1, 1, 0};

                        std::vector<float> vertexBufferData(5 * 4); // 8 entries per vertex * 4 vertices

                        float *buf = vertexBufferData.data();

                        for (int v = 0; v < 4; ++v, buf += 5)
                        {
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
#ifdef Q_OS_LINUX
                        if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI)
                        {
                            glGenTextures(2, d->m_textures);
                            for (const auto &texture : d->m_textures)
                            {
                                glBindTexture(GL_TEXTURE_2D, texture);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            }
                            d->m_program->bind();
                            d->m_program->setUniformValue("inputFormat", 1);
                            d->m_program->release();
                        }
                        else
#elif defined(Q_OS_WINDOWS)
                        if (d->m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD)
                        {
                            glGenTextures(1, d->m_textures);
                            glBindTexture(GL_TEXTURE_2D, d->m_textures[0]);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            d->m_program->bind();
                            d->m_program->setUniformValue("inputFormat", 0);
                            d->m_program->release();

                            auto surface = reinterpret_cast<LPDIRECT3DSURFACE9>(d->m_currentFrame->data[3]);
                            D3DSURFACE_DESC surfaceDesc;
                            surface->GetDesc(&surfaceDesc);
                            HANDLE hDevice;
                            IDirect3DDevice9 *pDevice;
                            d->m_pD3DManager->OpenDeviceHandle(&hDevice);
                            d->m_pD3DManager->LockDevice(hDevice, &pDevice, TRUE);

                            qWarning("Creating surface with %dx%d pixels", surfaceDesc.Width, surfaceDesc.Height);

                            pDevice->CreateOffscreenPlainSurface(surfaceDesc.Width, surfaceDesc.Height, D3DFMT_A8R8G8B8,
                                                                 D3DPOOL_DEFAULT, &d->m_pSharedSurface,
                                                                 &d->m_hSharedSurface);

                            d->m_hDXDevice = wglDXOpenDeviceNV(pDevice);
                            if (d->m_hDXDevice)
                            {
                                wglDXSetResourceShareHandleNV(d->m_pSharedSurface, d->m_hSharedSurface);
                                d->m_hSharedTexture = wglDXRegisterObjectNV(d->m_hDXDevice, d->m_pSharedSurface, d->m_textures[0],
                                                                            GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
                            }

                            pDevice->Release();
                            d->m_pD3DManager->UnlockDevice(hDevice, TRUE);
                        }
                        else if (d->m_currentFrame->format == AV_PIX_FMT_D3D11)
                        {
                            //                            qFatal("D3D11VA -> OpenGL interop is being worked on");
                            glGenTextures(1, d->m_textures);
                            glBindTexture(GL_TEXTURE_2D, d->m_textures[0]);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            d->m_program->bind();
                            d->m_program->setUniformValue("inputFormat", 0);
                            d->m_program->release();

                            D3D_FEATURE_LEVEL featureLevel;
                            UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if defined(DEBUG) || defined(_DEBUG)
                            flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

                            d->m_pD3D11Device = d->m_pD3D11VAContext->device;
                            d->m_pD3D11DeviceCtx = d->m_pD3D11VAContext->device_context;
                            // ASSERT(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &d->m_pD3D11Device, &featureLevel, &d->m_pD3D11DeviceCtx)));
                            // ASSERT(d->m_pD3D11Device && d->m_pD3D11DeviceCtx);

                            auto surface = reinterpret_cast<ID3D11Texture2D *>(d->m_currentFrame->data[0]);

                            //                            d->m_hDXDevice = wglDXOpenDeviceNV(d->m_pD3D11Device);

                            D3D11_TEXTURE2D_DESC desc, outDesc = {0}, inDesc = {0};
                            surface->GetDesc(&desc);
                            ZeroMemory(&outDesc, sizeof(D3D11_TEXTURE2D_DESC));
                            ZeroMemory(&inDesc, sizeof(D3D11_TEXTURE2D_DESC));
                            outDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                            outDesc.Width = desc.Width;
                            outDesc.Height = d->m_currentFrame->height;
                            outDesc.MipLevels = 1;
                            outDesc.ArraySize = 1;
                            outDesc.SampleDesc.Count = 1;
                            outDesc.SampleDesc.Quality = 0;
                            outDesc.Usage = D3D11_USAGE_DEFAULT;
                            outDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

                            inDesc.Format = desc.Format;
                            inDesc.Width = desc.Width;
                            inDesc.Height = d->m_currentFrame->height;
                            inDesc.MipLevels = 1;
                            inDesc.ArraySize = 1;
                            inDesc.SampleDesc.Count = 1;
                            inDesc.SampleDesc.Quality = 0;
                            inDesc.Usage = D3D11_USAGE_DEFAULT;
                            inDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;

                            ASSERT(SUCCEEDED(d->m_pD3D11Device->CreateTexture2D(&outDesc, nullptr, &d->m_pSharedTexture)));
                            ASSERT(SUCCEEDED(d->m_pD3D11Device->CreateTexture2D(&inDesc, nullptr, &d->m_pInputTexture)));

                            ASSERT(SUCCEEDED(d->m_pD3D11Device->QueryInterface(&d->m_pVideoDevice)));
                            ASSERT(SUCCEEDED(d->m_pD3D11DeviceCtx->QueryInterface(&d->m_pVideoDeviceCtx)));

                            D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpDesc;
                            ZeroMemory(&vpDesc, sizeof(D3D11_VIDEO_PROCESSOR_CONTENT_DESC));
                            vpDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
                            vpDesc.InputHeight = d->m_currentFrame->height;
                            vpDesc.InputWidth = desc.Width;
                            vpDesc.OutputHeight = d->m_currentFrame->height;
                            vpDesc.OutputWidth = desc.Width;
                            vpDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
                            ASSERT(SUCCEEDED(
                                d->m_pVideoDevice->CreateVideoProcessorEnumerator(&vpDesc, &d->m_pVideoProcEnum)));
                            D3D11_VIDEO_PROCESSOR_CAPS procCaps;
                            ASSERT(SUCCEEDED(d->m_pVideoProcEnum->GetVideoProcessorCaps(&procCaps)));
                            uint typeId = 0;
                            for (uint i = 0; i < procCaps.RateConversionCapsCount; ++i)
                            {
                                D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS convCaps;
                                ASSERT(SUCCEEDED(d->m_pVideoProcEnum->GetVideoProcessorRateConversionCaps(i, &convCaps)));
                                if ((convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_FRAME_RATE_CONVERSION) > 0)
                                {
                                    typeId = i;
                                    break;
                                }
                            }
                            ASSERT(SUCCEEDED(
                                d->m_pVideoDevice->CreateVideoProcessor(d->m_pVideoProcEnum, typeId, &d->m_pVideoProc)));

                            //                            // Output rate (repeat frames)
                            //                            pVideoContext->VideoProcessorSetStreamOutputRate(d->m_pVideoProc, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL,
                            //                                                                             TRUE, nullptr);
                            //                            // disable automatic video quality by driver
                            //                            pVideoContext->VideoProcessorSetStreamAutoProcessingMode(d->m_pVideoProc, 0, FALSE);
                            //                            // Output background color (black)
                            //                            static const D3D11_VIDEO_COLOR backgroundColor = {0.0f, 0.0f, 0.0f, 1.0f};
                            //                            pVideoContext->VideoProcessorSetOutputBackgroundColor(d->m_pVideoProc, FALSE, &backgroundColor);
                            //                            // other
                            //                            pVideoContext->VideoProcessorSetOutputTargetRect(d->m_pVideoProc, FALSE, nullptr);
                            //                            pVideoContext->VideoProcessorSetStreamRotation(d->m_pVideoProc, 0, FALSE,
                            //                                                                           D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY);

                            HRESULT hr;

                            {
                                UINT support;
                                d->m_pVideoProcEnum->CheckVideoProcessorFormat(desc.Format, &support);
                                qDebug() << "Input supported:"
                                         << ((support & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) > 0 ? "true" : "false");
                                d->m_pVideoProcEnum->CheckVideoProcessorFormat(DXGI_FORMAT_R8G8B8A8_UNORM, &support);
                                qDebug() << "Output supported:"
                                         << ((support & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) > 0 ? "true" : "false");
                                //                                hr = d->m_pVideoDevice->CreateVideoProcessorOutputView(d->m_pSharedTexture,
                                //                                                                                                        d->m_pVideoProcEnum,
                                //                                                                                                        &outputViewDesc,
                                //                                                                                                        nullptr);
                                //                                if (FAILED(hr)) {
                                //                                    _com_error err(hr);
                                //                                    qFatal(err.ErrorMessage());
                                //                                }
                                D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {D3D11_VPOV_DIMENSION_TEXTURE2D};
                                outputViewDesc.Texture2D.MipSlice = 0;
                                outputViewDesc.Texture2DArray.MipSlice = 0;
                                outputViewDesc.Texture2DArray.FirstArraySlice = 0;
                                outputViewDesc.Texture2DArray.ArraySize = 1;
                                d->m_pSharedTexture->GetDesc(&outDesc);
                                //ASSERT((static_cast<D3D11_BIND_FLAG>(outDesc.BindFlags) & D3D11_BIND_RENDER_TARGET) >= 1);
                                qDebug() << "D3D11_BIND_RENDER_TARGET:" << D3D11_BIND_RENDER_TARGET;
                                D3D11_BIND_FLAG flag2 = static_cast<D3D11_BIND_FLAG>(512);
                                ASSERT(SUCCEEDED(d->m_pVideoDevice->CreateVideoProcessorOutputView(d->m_pSharedTexture,
                                                                                                   d->m_pVideoProcEnum,
                                                                                                   &outputViewDesc,
                                                                                                   &d->m_pVideoProcOutputView)));
                            }

                            {
                                D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc;
                                ZeroMemory(&inputViewDesc, sizeof(inputViewDesc));
                                inputViewDesc.FourCC = 0;
                                inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
                                inputViewDesc.Texture2D.MipSlice = 0;
                                inputViewDesc.Texture2D.ArraySlice = 0;
                                qDebug("Texture array size: %d", desc.ArraySize);
                                ASSERT(SUCCEEDED(
                                    d->m_pVideoDevice->CreateVideoProcessorInputView(d->m_pInputTexture, d->m_pVideoProcEnum,
                                                                                     &inputViewDesc,
                                                                                     &d->m_pVideoProcInputView)));
                            }

                            d->m_hDXDevice = wglDXOpenDeviceNV(d->m_pD3D11VAContext->device);

                            if (d->m_hDXDevice)
                            {
                                d->m_hSharedTexture = wglDXRegisterObjectNV(d->m_hDXDevice, d->m_pSharedTexture, d->m_textures[0],
                                                                            GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
                            }
                        }
                        else
#endif
                        {
                            bool VTexActive = false, UTexActive = false;
                            QSize YSize{-1, -1}, USize{-1, -1}, VSize{-1, -1};
                            QOpenGLTexture::TextureFormat textureFormatY{}, textureFormatU{}, textureFormatV{};
                            QOpenGLTexture::PixelFormat pixelFormatY{}, pixelFormatU{}, pixelFormatV{};
                            QOpenGLTexture::PixelType pixelType{};
                            switch (static_cast<AVPixelFormat>(d->m_currentFrame->format))
                            {
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
                            if (UTexActive)
                            {
                                d->m_uTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
                                d->m_uTexture->setSize(USize.width(), USize.height());
                                d->m_uTexture->setFormat(textureFormatU);
                                d->m_uTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                                d->m_uTexture->allocateStorage(pixelFormatU, pixelType);
                            }
                            if (VTexActive)
                            {
                                d->m_vTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
                                d->m_vTexture->setSize(VSize.width(), VSize.height());
                                d->m_vTexture->setFormat(textureFormatV);
                                d->m_vTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                                d->m_vTexture->allocateStorage(pixelFormatV, pixelType);
                            }
                        }
                    }
#ifdef Q_OS_LINUX
                    if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI)
                    {
                        for (int i = 0; i < 2; ++i)
                        {
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
                            VA_STATUS_SUCCESS)
                        {
                            qFatal("[AVQt::OpenGLRenderer] Could not export VA surface handle");
                        }
                        vaSyncSurface(d->m_VADisplay, va_surface);

                        static uint32_t formats[2];
                        char strBuf[AV_FOURCC_MAX_STRING_SIZE];
                        switch (prime.fourcc)
                        {
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

                        for (int i = 0; i < 2; ++i)
                        {
                            if (prime.layers[i].drm_format != formats[i])
                            {
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
                            d->m_EGLImages[i] = eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                                                  img_attr);

                            auto error = eglGetError();
                            if (!d->m_EGLImages[i] || d->m_EGLImages[i] == EGL_NO_IMAGE_KHR || error != EGL_SUCCESS)
                            {
                                qFatal("[AVQt::OpenGLRenderer] Could not create %s EGLImage: %s", (i ? "UV" : "Y"),
                                       eglErrorString(error).c_str());
                            }
#undef LAYER
#undef PLANE

                            glActiveTexture(GL_TEXTURE0 + i);
                            glBindTexture(GL_TEXTURE_2D, d->m_textures[i]);
                            while (glGetError() != GL_NO_ERROR)
                                ;
                            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, d->m_EGLImages[i]);
                            auto err = glGetError();

                            if (err != GL_NO_ERROR)
                            {
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
                        for (int i = 0; i < (int)prime.num_objects; ++i)
                        {
                            closefd(prime.objects[i].fd);
                        }
                    }
                    else
#elif defined(Q_OS_WINDOWS)
                    if (d->m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD)
                    {
                        auto surface = reinterpret_cast<LPDIRECT3DSURFACE9>(d->m_currentFrame->data[3]);
                        D3DSURFACE_DESC surfaceDesc;
                        surface->GetDesc(&surfaceDesc);
                        HANDLE hDevice;
                        d->m_pD3DManager->OpenDeviceHandle(&hDevice);
                        IDirect3DDevice9 *pDevice;
                        d->m_pD3DManager->LockDevice(hDevice, &pDevice, true);
                        IDirect3D9 *d3d9;
                        pDevice->GetDirect3D(&d3d9);
                        ASSERT(SUCCEEDED(d3d9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, surfaceDesc.Format,
                                                                           D3DFMT_A8R8G8B8)));
                        if (0 != pDevice->StretchRect(surface, nullptr, d->m_pSharedSurface, nullptr, D3DTEXF_LINEAR))
                        {
                            qFatal("Could not map NV12/P010 surface to RGB");
                        }

                        //                        auto image = SurfaceToQImage(surface/*, pDevice*/, d->m_currentFrame->height);
                        //                        if (!image.isNull()) {
                        //                            image.save("output.bmp");
                        //                        }

                        d->m_pD3DManager->UnlockDevice(hDevice, TRUE);
                        d->m_pD3DManager->CloseDeviceHandle(hDevice);
                        //                        auto image = SurfaceToQImage(surface, frame->height);
                        //                        qDebug() << "Decoded surface is using" << image.sizeInBytes() << "Byte of memory at" << image.size();
                        //                        image.save("decoded.bmp");
                        //                        image = SurfaceToQImage(d->m_pSharedSurface, frame->height);
                        //                        qDebug() << "Mapped surface is using" << image.sizeInBytes() << "Byte of memory at" << image.size();
                        //                        image.save("mapped.bmp");
                        //                        exit(0);
                    }
                    else if (d->m_currentFrame->format == AV_PIX_FMT_D3D11)
                    {
                        auto surface = reinterpret_cast<ID3D11Texture2D *>(d->m_currentFrame->data[0]);
                        auto index = reinterpret_cast<intptr_t>(d->m_currentFrame->data[1]);
                        D3D11_TEXTURE2D_DESC desc;
                        surface->GetDesc(&desc);
                        HRESULT hr;

                        D3D11_BOX box;
                        box.top = 0;
                        box.left = 0;
                        box.right = desc.Width;
                        box.bottom = d->m_currentFrame->height;
                        box.front = 0;
                        box.back = 1;
                        d->m_pD3D11DeviceCtx->CopySubresourceRegion(d->m_pInputTexture, 0, 0, 0, 0, surface, index, &box);

                        D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
                        streams.Enable = TRUE;
                        streams.pInputSurface = d->m_pVideoProcInputView;
                        hr = d->m_pVideoDeviceCtx->VideoProcessorBlt(d->m_pVideoProc, d->m_pVideoProcOutputView, 0, 1, &streams);
                        if (FAILED(hr))
                        {
                            _com_error err(hr);
                            qFatal(err.ErrorMessage());
                        }
                        ASSERT(SUCCEEDED(hr));
                    }
                    else
#endif
                    {
                        //                    qDebug("Frame duration: %ld ms", d->m_currentFrameTimeout);
                        if (differentPixFmt)
                        {
                            d->m_program->bind();
                        }
                        d->m_yTexture->bind(0);
                        if (d->m_uTexture)
                        {
                            d->m_uTexture->bind(1);
                        }
                        if (d->m_vTexture)
                        {
                            d->m_vTexture->bind(2);
                        }
                        switch (d->m_currentFrame->format)
                        {
                        case AV_PIX_FMT_BGRA:
                            d->m_yTexture->setData(QOpenGLTexture::PixelFormat::BGRA, QOpenGLTexture::UInt8,
                                                   const_cast<const uint8_t *>(d->m_currentFrame->data[0]));
                            if (differentPixFmt)
                            {
                                d->m_program->setUniformValue("inputFormat", 0);
                            }
                            break;
                        case AV_PIX_FMT_NV12:
                            d->m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                                                   const_cast<const uint8_t *>(d->m_currentFrame->data[0]));
                            d->m_uTexture->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt8,
                                                   const_cast<const uint8_t *>(d->m_currentFrame->data[1]));
                            if (differentPixFmt)
                            {
                                d->m_program->setUniformValue("inputFormat", 1);
                            }
                            break;
                        case AV_PIX_FMT_P010:
                            d->m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16,
                                                   const_cast<const uint8_t *>(d->m_currentFrame->data[0]));
                            d->m_uTexture->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt16,
                                                   const_cast<const uint8_t *>(d->m_currentFrame->data[1]));
                            if (differentPixFmt)
                            {
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
                            if (differentPixFmt)
                            {
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
                            if (differentPixFmt)
                            {
                                d->m_program->setUniformValue("inputFormat", 3);
                            }
                            break;
                        default:
                            qFatal("Pixel format not supported");
                        }
                        if (differentPixFmt)
                        {
                            d->m_program->release();
                        }
                    }
                }
            }
        }
        else if (d->m_clock)
        {
            if (d->m_clock->isActive())
            {
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

        if (d->m_currentFrame)
        {
            qDebug("Drawing frame with PTS: %lld", static_cast<long long>(d->m_currentFrame->pts));
            d->m_program->bind();
#ifdef Q_OS_LINUX
            if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, d->m_textures[0]);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, d->m_textures[1]);
            }
            else
#elif defined(Q_OS_WINDOWS)
            if (d->m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD || d->m_currentFrame->format == AV_PIX_FMT_D3D11)
            {
                if (!wglDXLockObjectsNV(d->m_hDXDevice, 1, &d->m_hSharedTexture))
                {
                    qFatal(GetLastErrorAsString().c_str());
                }
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, d->m_textures[0]);
            }
            else
#endif
            {
                if (!d->m_yTexture->isBound(0))
                {
                    d->m_yTexture->bind(0);
                    if (d->m_uTexture)
                    {
                        d->m_uTexture->bind(1);
                    }
                    if (d->m_vTexture)
                    {
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
#ifdef Q_OS_LINUX
            if (d->m_currentFrame->format == AV_PIX_FMT_VAAPI)
            {
            }
#elif defined(Q_OS_WINDOWS)
            else if (d->m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD || d->m_currentFrame->format == AV_PIX_FMT_D3D11)
            {
                wglDXUnlockObjectsNV(d->m_hDXDevice, 1, &d->m_hSharedTexture);
            }
#endif
            else
            {
                d->m_yTexture->release(0);
                if (d->m_uTexture)
                {
                    d->m_uTexture->release(1);
                }
                if (d->m_vTexture)
                {
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

        if (d->m_currentFrame)
        {
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
        if (!d->m_paused.load())
        {
            update();
        }
    }

    void OpenGLRenderer::mouseReleaseEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton)
        {
            pause(nullptr, !isPaused());
        }
    }

    void OpenGLRenderer::closeEvent(QCloseEvent *event)
    {
        QApplication::quit();
        QWidget::closeEvent(event);
    }

    RenderClock *OpenGLRenderer::getClock()
    {
        Q_D(AVQt::OpenGLRenderer);
        return d->m_clock;
    }

    [[maybe_unused]] GLint
    OpenGLRendererPrivate::project(GLdouble objx, GLdouble objy, GLdouble objz, const GLdouble model[16], const GLdouble proj[16],
                                   const GLint viewport[4], GLdouble *winx, GLdouble *winy, GLdouble *winz)
    {
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

    void OpenGLRendererPrivate::transformPoint(GLdouble *out, const GLdouble *m, const GLdouble *in)
    {
#define M(row, col) m[(col)*4 + (row)]
        out[0] = M(0, 0) * in[0] + M(0, 1) * in[1] + M(0, 2) * in[2] + M(0, 3) * in[3];
        out[1] = M(1, 0) * in[0] + M(1, 1) * in[1] + M(1, 2) * in[2] + M(1, 3) * in[3];
        out[2] = M(2, 0) * in[0] + M(2, 1) * in[1] + M(2, 2) * in[2] + M(2, 3) * in[3];
        out[3] = M(3, 0) * in[0] + M(3, 1) * in[1] + M(3, 2) * in[2] + M(3, 3) * in[3];
#undef M
    }

    QTime OpenGLRendererPrivate::timeFromMillis(int64_t ts)
    {
        int ms = static_cast<int>(ts % 1000);
        int s = static_cast<int>((ts / 1000) % 60);
        int m = static_cast<int>((ts / 1000 / 60) % 60);
        int h = static_cast<int>(ts / 1000 / 60 / 60);
        return {h, m, s, ms};
    }
}