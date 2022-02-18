// Copyright (c) 2021-2022.
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
// Created by silas on 19.12.21.
//

#include "global.hpp"
#include "AVQt/decoder/VideoDecoderFactory.hpp"
#include "AVQt/encoder/VideoEncoderFactory.hpp"
#include "AVQt/renderers/OpenGLFrameMapperFactory.hpp"
#include "AVQt/capture/DesktopCaptureFactory.hpp"

void load_resources_impl() {
    Q_INIT_RESOURCE(AVQtShader);
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
    void loadResources() {
        static std::atomic_bool loaded{false};
        bool shouldBe = false;
        if (loaded.compare_exchange_strong(shouldBe, true)) {
            load_resources_impl();
        }
    }

    void registerMetatypes() {
        qRegisterMetaType<std::shared_ptr<AVPacket>>();
        qRegisterMetaType<std::shared_ptr<AVFrame>>();
    }

    AVCodecID getCodecId(Codec codec) {
        switch (codec) {
            case Codec::H264:
                return AV_CODEC_ID_H264;
            case Codec::HEVC:
                return AV_CODEC_ID_HEVC;
            case Codec::VP8:
                return AV_CODEC_ID_VP8;
            case Codec::VP9:
                return AV_CODEC_ID_VP9;
            case Codec::MPEG2:
                return AV_CODEC_ID_MPEG2VIDEO;
            default:
                return AV_CODEC_ID_NONE;
        }
    }
}// namespace AVQt
