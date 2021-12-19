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

#include "renderers/private/OpenGLRendererOld_p.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <GL/GLU.h>
#include <QtConcurrent>
#include <QtCore>

#include <comdef.h>
#include <d3d9.h>
#define WGL_LOOKUP_FUNCTION(type, func)           \
    auto(func) = (type) wglGetProcAddress(#func); \
    if (!(func)) {                                \
        qFatal("wglGetProcAddress(" #func ")");   \
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
//#define WGL_ACCESS_READ_WRITE_NV 0x0001 // Not needed by code, but left here for completeness
//#define WGL_ACCESS_WRITE_DISCARD_NV 0x0002

#undef ASSERT
#define ASSERT(expr)                              \
    if (!(expr)) {                                \
        qFatal("Assertion \"" #expr "\" failed"); \
    }

std::string GetLastErrorAsString() {
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return {};//No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 nullptr, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuffer, 0, nullptr);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

[[maybe_unused]] void saveTexToFile(ID3D11Texture2D *texture, ID3D11Device *pDevice, ID3D11DeviceContext *pDeviceCtx, const QString &filename) {
    // Create staging texture with the exact same dimensions and format like the original one
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    ID3D11Texture2D *staging;
    pDevice->CreateTexture2D(&desc, nullptr, &staging);

    // Copy data in GPU memory
    pDeviceCtx->CopySubresourceRegion(staging, 0, 0, 0, 0, texture, 0, nullptr);

    // Make data accessible by CPU
    D3D11_MAPPED_SUBRESOURCE mapped;
    pDeviceCtx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);

    // Save it
    QImage image(reinterpret_cast<uint8_t *>(mapped.pData), static_cast<int>(desc.Width), static_cast<int>(desc.Height), mapped.RowPitch, QImage::Format_ARGB32);
    image.save(filename);

    // Clean up
    pDeviceCtx->Unmap(staging, 0);
    staging->Release();
}

namespace AVQt {
    std::atomic_bool OpenGLRendererPrivate::resourcesLoaded{false};

    OpenGLRendererPrivate::OpenGLRendererPrivate(OpenGLRenderer *q) : q_ptr(q) {
    }

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
                            if (m_firstFrame.compare_exchange_strong(shouldBe, false)) {
                                if (input->format == AV_PIX_FMT_DXVA2_VLD) {
                                    m_pDXVAContext = static_cast<AVDXVA2DeviceContext *>(reinterpret_cast<AVHWDeviceContext *>(pDeviceCtx->data)->hwctx);
                                    m_pD3DManager = m_pDXVAContext->devmgr;
                                    m_pD3DManager->AddRef();
                                } else if (input->format == AV_PIX_FMT_D3D11) {
                                    m_pD3D11VAContext = static_cast<AVD3D11VADeviceContext *>(reinterpret_cast<AVHWDeviceContext *>(pDeviceCtx->data)->hwctx);
                                }
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
    }

    void OpenGLRendererPrivate::initializePlatformAPI() {
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
    }

    void OpenGLRendererPrivate::initializeOnFirstFrame() {
        WGL_LOOKUP_FUNCTION(PFNWGLDXOPENDEVICENVPROC, wglDXOpenDeviceNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXREGISTEROBJECTNVPROC, wglDXRegisterObjectNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXSETRESOURCESHAREHANDLENVPROC, wglDXSetResourceShareHandleNV)
        // Frame has 64 pixel alignment, set max height coord to cut off additional pixels
        float maxTexHeight = 1.0f;
        if (m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD) {
            auto surface = (LPDIRECT3DSURFACE9) m_currentFrame->data[3];
            D3DSURFACE_DESC surfaceDesc;
            surface->GetDesc(&surfaceDesc);
            maxTexHeight = static_cast<float>(m_currentFrame->height * 1.0 / surfaceDesc.Height);
        } else if (m_currentFrame->format == AV_PIX_FMT_D3D11) {
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
                0, 1, 3,// first triangle
                1, 2, 3 // second triangle
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

        if (m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD) {
            glGenTextures(1, m_textures);
            glBindTexture(GL_TEXTURE_2D, m_textures[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            m_program->bind();
            m_program->setUniformValue("inputFormat", 0);
            m_program->release();

            auto surface = reinterpret_cast<LPDIRECT3DSURFACE9>(m_currentFrame->data[3]);
            D3DSURFACE_DESC surfaceDesc;
            surface->GetDesc(&surfaceDesc);
            HANDLE hDevice;
            IDirect3DDevice9 *pDevice;
            m_pD3DManager->OpenDeviceHandle(&hDevice);
            m_pD3DManager->LockDevice(hDevice, &pDevice, TRUE);

            qWarning("Creating surface with %dx%d pixels", surfaceDesc.Width, surfaceDesc.Height);

            pDevice->CreateOffscreenPlainSurface(surfaceDesc.Width, surfaceDesc.Height, D3DFMT_A8R8G8B8,
                                                 D3DPOOL_DEFAULT, &m_pSharedSurface,
                                                 &m_hSharedSurface);

            m_hDXDevice = wglDXOpenDeviceNV(pDevice);
            if (m_hDXDevice) {
                wglDXSetResourceShareHandleNV(m_pSharedSurface, m_hSharedSurface);
                m_hSharedTexture = wglDXRegisterObjectNV(m_hDXDevice, m_pSharedSurface, m_textures[0],
                                                         GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
            }

            pDevice->Release();
            m_pD3DManager->UnlockDevice(hDevice, TRUE);
        } else if (m_currentFrame->format == AV_PIX_FMT_D3D11) {
            glGenTextures(1, m_textures);
            glBindTexture(GL_TEXTURE_2D, m_textures[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            m_program->bind();
            m_program->setUniformValue("inputFormat", 0);
            m_program->release();

            m_pD3D11Device = m_pD3D11VAContext->device;
            m_pD3D11DeviceCtx = m_pD3D11VAContext->device_context;

            auto surface = reinterpret_cast<ID3D11Texture2D *>(m_currentFrame->data[0]);

            D3D11_TEXTURE2D_DESC desc, outDesc = {0}, inDesc = {0};
            surface->GetDesc(&desc);
            ZeroMemory(&outDesc, sizeof(D3D11_TEXTURE2D_DESC));
            ZeroMemory(&inDesc, sizeof(D3D11_TEXTURE2D_DESC));
            outDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            outDesc.Width = desc.Width;
            outDesc.Height = m_currentFrame->height;
            outDesc.MipLevels = 1;
            outDesc.ArraySize = 1;
            outDesc.SampleDesc.Count = 1;
            outDesc.SampleDesc.Quality = 0;
            outDesc.Usage = D3D11_USAGE_DEFAULT;
            outDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

            inDesc.Format = desc.Format;
            inDesc.Width = desc.Width;
            inDesc.Height = m_currentFrame->height;
            inDesc.MipLevels = 1;
            inDesc.ArraySize = 1;
            inDesc.SampleDesc.Count = 1;
            inDesc.SampleDesc.Quality = 0;
            inDesc.Usage = D3D11_USAGE_DEFAULT;
            inDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;

            ASSERT(SUCCEEDED(m_pD3D11Device->CreateTexture2D(&outDesc, nullptr, &m_pSharedTexture)));
            ASSERT(SUCCEEDED(m_pD3D11Device->CreateTexture2D(&inDesc, nullptr, &m_pInputTexture)));

            ASSERT(SUCCEEDED(m_pD3D11Device->QueryInterface(&m_pVideoDevice)));
            ASSERT(SUCCEEDED(m_pD3D11DeviceCtx->QueryInterface(&m_pVideoDeviceCtx)));

            D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpDesc;
            ZeroMemory(&vpDesc, sizeof(D3D11_VIDEO_PROCESSOR_CONTENT_DESC));
            vpDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
            vpDesc.InputHeight = m_currentFrame->height;
            vpDesc.InputWidth = desc.Width;
            vpDesc.OutputHeight = m_currentFrame->height;
            vpDesc.OutputWidth = desc.Width;
            vpDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
            ASSERT(SUCCEEDED(
                    m_pVideoDevice->CreateVideoProcessorEnumerator(&vpDesc, &m_pVideoProcEnum)));
            D3D11_VIDEO_PROCESSOR_CAPS procCaps;
            ASSERT(SUCCEEDED(m_pVideoProcEnum->GetVideoProcessorCaps(&procCaps)));
            uint typeId = 0;
            for (uint i = 0; i < procCaps.RateConversionCapsCount; ++i) {
                D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS convCaps;
                ASSERT(SUCCEEDED(m_pVideoProcEnum->GetVideoProcessorRateConversionCaps(i, &convCaps)));
                if ((convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_FRAME_RATE_CONVERSION) > 0) {
                    typeId = i;
                    break;
                }
            }
            ASSERT(SUCCEEDED(m_pVideoDevice->CreateVideoProcessor(m_pVideoProcEnum, typeId, &m_pVideoProc)));

            {
                UINT support;
                m_pVideoProcEnum->CheckVideoProcessorFormat(desc.Format, &support);
                qDebug() << "Input supported:"
                         << ((support & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) > 0 ? "true" : "false");
                m_pVideoProcEnum->CheckVideoProcessorFormat(DXGI_FORMAT_R8G8B8A8_UNORM, &support);
                qDebug() << "Output supported:"
                         << ((support & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) > 0 ? "true" : "false");
                D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {D3D11_VPOV_DIMENSION_TEXTURE2D};
                outputViewDesc.Texture2D.MipSlice = 0;
                outputViewDesc.Texture2DArray.MipSlice = 0;
                outputViewDesc.Texture2DArray.FirstArraySlice = 0;
                outputViewDesc.Texture2DArray.ArraySize = 1;
                m_pSharedTexture->GetDesc(&outDesc);
                ASSERT(SUCCEEDED(m_pVideoDevice->CreateVideoProcessorOutputView(m_pSharedTexture,
                                                                                m_pVideoProcEnum,
                                                                                &outputViewDesc,
                                                                                &m_pVideoProcOutputView)));
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
                        m_pVideoDevice->CreateVideoProcessorInputView(m_pInputTexture, m_pVideoProcEnum,
                                                                      &inputViewDesc,
                                                                      &m_pVideoProcInputView)));
            }

            m_hDXDevice = wglDXOpenDeviceNV(m_pD3D11VAContext->device);

            if (m_hDXDevice) {
                m_hSharedTexture = wglDXRegisterObjectNV(m_hDXDevice, m_pSharedTexture, m_textures[0],
                                                         GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
            }
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
    }

    void OpenGLRendererPrivate::updatePixelFormat() {
        qDebug("Update pixel format :%s", av_get_pix_fmt_name(static_cast<AVPixelFormat>(m_currentFrame->format)));
        m_program->bind();
        switch (m_currentFrame->format) {
            case AV_PIX_FMT_BGRA:
                m_program->setUniformValue("inputFormat", 0);
                break;
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
    }

    void OpenGLRendererPrivate::mapFrame() {
        WGL_LOOKUP_FUNCTION(PFNWGLDXOPENDEVICENVPROC, wglDXOpenDeviceNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXCLOSEDEVICENVPROC, wglDXCloseDeviceNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXREGISTEROBJECTNVPROC, wglDXRegisterObjectNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXUNREGISTEROBJECTNVPROC, wglDXUnregisterObjectNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXLOCKOBJECTSNVPROC, wglDXLockObjectsNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXUNLOCKOBJECTSNVPROC, wglDXUnlockObjectsNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXOBJECTACCESSNVPROC, wglDXObjectAccessNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXSETRESOURCESHAREHANDLENVPROC, wglDXSetResourceShareHandleNV)
        WGL_LOOKUP_FUNCTION(PFNGLACTIVETEXTUREPROC, glActiveTexture)

        if (m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD) {
            auto surface = reinterpret_cast<LPDIRECT3DSURFACE9>(m_currentFrame->data[3]);
            D3DSURFACE_DESC surfaceDesc;
            surface->GetDesc(&surfaceDesc);
            HANDLE hDevice;
            m_pD3DManager->OpenDeviceHandle(&hDevice);
            IDirect3DDevice9 *pDevice;
            m_pD3DManager->LockDevice(hDevice, &pDevice, true);
            IDirect3D9 *d3d9;
            pDevice->GetDirect3D(&d3d9);
            ASSERT(SUCCEEDED(d3d9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, surfaceDesc.Format,
                                                               D3DFMT_A8R8G8B8)));
            if (0 != pDevice->StretchRect(surface, nullptr, m_pSharedSurface, nullptr, D3DTEXF_LINEAR)) {
                qFatal("Could not stretch NV12/P010 surface to RGB surface");
            }
            m_pD3DManager->UnlockDevice(hDevice, TRUE);
            m_pD3DManager->CloseDeviceHandle(hDevice);
        } else if (m_currentFrame->format == AV_PIX_FMT_D3D11) {
            auto surface = reinterpret_cast<ID3D11Texture2D *>(m_currentFrame->data[0]);
            auto index = reinterpret_cast<intptr_t>(m_currentFrame->data[1]);
            D3D11_TEXTURE2D_DESC desc;
            surface->GetDesc(&desc);
            HRESULT hr;

            D3D11_BOX box;
            box.top = 0;
            box.left = 0;
            box.right = desc.Width;
            box.bottom = m_currentFrame->height;
            box.front = 0;
            box.back = 1;
            m_pD3D11DeviceCtx->CopySubresourceRegion(m_pInputTexture, 0, 0, 0, 0, surface, index, &box);

            D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
            streams.Enable = TRUE;
            streams.pInputSurface = m_pVideoProcInputView;
            hr = m_pVideoDeviceCtx->VideoProcessorBlt(m_pVideoProc, m_pVideoProcOutputView, 0, 1, &streams);
            if (FAILED(hr)) {
                _com_error err(hr);
                qFatal(err.ErrorMessage());
            }
            ASSERT(SUCCEEDED(hr));

            //            saveTexToFile(m_pSharedTexture, m_pD3D11Device, m_pD3D11DeviceCtx, "output_dxtex.bmp");
        } else {
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
    }

    void OpenGLRendererPrivate::bindResources() {
        WGL_LOOKUP_FUNCTION(PFNWGLDXOPENDEVICENVPROC, wglDXOpenDeviceNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXCLOSEDEVICENVPROC, wglDXCloseDeviceNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXREGISTEROBJECTNVPROC, wglDXRegisterObjectNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXUNREGISTEROBJECTNVPROC, wglDXUnregisterObjectNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXLOCKOBJECTSNVPROC, wglDXLockObjectsNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXUNLOCKOBJECTSNVPROC, wglDXUnlockObjectsNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXOBJECTACCESSNVPROC, wglDXObjectAccessNV)
        WGL_LOOKUP_FUNCTION(PFNWGLDXSETRESOURCESHAREHANDLENVPROC, wglDXSetResourceShareHandleNV)
        WGL_LOOKUP_FUNCTION(PFNGLACTIVETEXTUREPROC, glActiveTexture)

        if (m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD || m_currentFrame->format == AV_PIX_FMT_D3D11) {
            if (!wglDXLockObjectsNV(m_hDXDevice, 1, &m_hSharedTexture)) {
                qFatal(GetLastErrorAsString().c_str());
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_textures[0]);
        } else {
            if (m_yTexture) {
                m_yTexture->bind(0);
            }
            if (m_uTexture) {
                m_uTexture->bind(1);
            }
            if (m_vTexture) {
                m_vTexture->bind(2);
            }
        }

        m_vao.bind();
        m_ibo.bind();
        m_program->bind();
    }

    void OpenGLRendererPrivate::releaseResources() {
        WGL_LOOKUP_FUNCTION(PFNWGLDXUNLOCKOBJECTSNVPROC, wglDXUnlockObjectsNV)
        m_vao.release();
        m_vbo.release();
        m_program->release();

        if (m_currentFrame->format == AV_PIX_FMT_DXVA2_VLD || m_currentFrame->format == AV_PIX_FMT_D3D11) {
            wglDXUnlockObjectsNV(m_hDXDevice, 1, &m_hSharedTexture);
            auto err = glGetError();
            if (err != GL_NO_ERROR) {
                qWarning("Error in OpenGL: %s", gluErrorStringWIN(err));
            }
        } else {
            m_yTexture->release(0);
            auto err = glGetError();
            if (err != GL_NO_ERROR) {
                qWarning("Error in OpenGL: %s", gluErrorStringWIN(err));
            }
            if (m_uTexture) {
                m_uTexture->release(1);
                err = glGetError();
                if (err != GL_NO_ERROR) {
                    qWarning("Error in OpenGL: %s", gluErrorStringWIN(err));
                }
            }
            if (m_vTexture) {
                m_vTexture->release(2);
                err = glGetError();
                if (err != GL_NO_ERROR) {
                    qWarning("Error in OpenGL: %s", gluErrorStringWIN(err));
                }
            }
        }
    }

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
            if (m_context->isValid()) {
                if (m_context->makeCurrent(m_surface)) {
                    WGL_LOOKUP_FUNCTION(PFNWGLDXCLOSEDEVICENVPROC, wglDXCloseDeviceNV)
                    WGL_LOOKUP_FUNCTION(PFNWGLDXREGISTEROBJECTNVPROC, wglDXRegisterObjectNV)
                    WGL_LOOKUP_FUNCTION(PFNWGLDXUNREGISTEROBJECTNVPROC, wglDXUnregisterObjectNV)
                    WGL_LOOKUP_FUNCTION(PFNWGLDXLOCKOBJECTSNVPROC, wglDXLockObjectsNV)
                    WGL_LOOKUP_FUNCTION(PFNWGLDXUNLOCKOBJECTSNVPROC, wglDXUnlockObjectsNV)
                    WGL_LOOKUP_FUNCTION(PFNWGLDXOBJECTACCESSNVPROC, wglDXObjectAccessNV)
                    WGL_LOOKUP_FUNCTION(PFNWGLDXSETRESOURCESHAREHANDLENVPROC, wglDXSetResourceShareHandleNV)
                    WGL_LOOKUP_FUNCTION(PFNGLACTIVETEXTUREPROC, glActiveTexture)
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

                    if (m_hDXDevice) {
                        wglDXUnregisterObjectNV(m_hDXDevice, m_hSharedTexture);
                        wglDXCloseDeviceNV(m_hDXDevice);
                        m_hSharedTexture = nullptr;
                        m_hDXDevice = nullptr;
                    }
                    m_context->doneCurrent();
                }
            }
        }

        if (m_pD3DManager) {
            HANDLE hDevice;
            IDirect3DDevice9 *pDevice;
            m_pD3DManager->OpenDeviceHandle(&hDevice);
            m_pD3DManager->LockDevice(hDevice, &pDevice, TRUE);
            m_pSharedSurface->Release();
            m_pD3DManager->UnlockDevice(hDevice, FALSE);
            m_pD3DManager->CloseDeviceHandle(hDevice);
            m_pD3DManager->Release();
        }
        if (m_pVideoProcOutputView) {
            m_pVideoProcOutputView->Release();
        }
        if (m_pVideoProcInputView) {
            m_pVideoProcInputView->Release();
        }
        if (m_pVideoProc) {
            m_pVideoProc->Release();
        }
        if (m_pVideoProcEnum) {
            m_pVideoProcEnum->Release();
        }
        if (m_pSharedTexture) {
            m_pSharedTexture->Release();
        }
        if (m_pVideoDevice) {
            m_pVideoDevice->Release();
        }
        if (m_pVideoDeviceCtx) {
            m_pVideoDeviceCtx->Release();
        }
        if (m_pD3D11Device) {
            m_pD3D11Device->Release();
        }
        if (m_pD3D11DeviceCtx) {
            m_pD3D11DeviceCtx->Release();
        }
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
#define M(row, col) m[(col) *4 + (row)]
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
        return {h, m, s, ms};
    }
}// namespace AVQt
