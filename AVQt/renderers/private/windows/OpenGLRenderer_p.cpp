//
// Created by silas on 14.09.21.
//

#include "renderers/private/OpenGLRenderer_p.h"

#include <comdef.h>
#include <d3d9.h>

#define WGL_LOOKUP_FUNCTION(type, func)          \
    type func = (type) wglGetProcAddress(#func); \
    if (!(func)) {                               \
        qFatal("wglGetProcAddress(" #func ")");  \
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
    if (!expr) {                                  \
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

[[maybe_unused]] QImage SurfaceToQImage(IDirect3DSurface9 *pSurface /*, IDirect3DDevice9 *pDevice*/, int height) {
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
    switch (surfaceDesc.Format) {
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
    for (uint y = 0; y < surfaceDesc.Height; ++y) {
        qDebug() << y << ": From" << ((uint8_t *) locked.pBits) + y * locked.Pitch << ", To:"
                 << result.scanLine(y) << ", Size:" << surfaceDesc.Width * bpp;
        memcpy(result.scanLine(y), ((uint8_t *) locked.pBits) + y * locked.Pitch,
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