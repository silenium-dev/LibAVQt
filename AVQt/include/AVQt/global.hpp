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

#ifndef LIBAVQT_GLOBAL_H
#define LIBAVQT_GLOBAL_H

#include "AVQt/communication/AudioPadParams.hpp"
#include "AVQt/communication/PacketPadParams.hpp"
#include "AVQt/communication/VideoPadParams.hpp"
#include "AVQt/encoder/IAudioEncoderImpl.hpp"
#include "AVQt/encoder/IVideoEncoderImpl.hpp"

#include <pgraph/impl/SimpleConsumer.hpp>
#include <pgraph/impl/SimpleProducer.hpp>

#include <QMetaType>
#include <qglobal.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#ifdef AVQT_LIBRARY_BUILD
#define AVQT_DEPRECATED
#else
#define AVQT_DEPRECATED Q_DECL_DEPRECATED
#endif

#ifdef Q_OS_LINUX
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define LOOKUP_FUNCTION(type, func)               \
    auto(func) = (type) eglGetProcAddress(#func); \
    if (!(func)) {                                \
        qFatal("eglGetProcAddress(" #func ")");   \
    }

std::string eglErrorString(EGLint error);
#endif

namespace AVQt {
    void loadResources();
    void registerMetatypes();
    AVCodecID getVideoCodecId(VideoCodec codec);
    AVCodecID getAudioCodecId(AudioCodec codec);
}// namespace AVQt

class QOpenGLFramebufferObject;

Q_DECLARE_METATYPE(pgraph::impl::SimpleProducer *)
Q_DECLARE_METATYPE(pgraph::impl::SimpleConsumer *)
Q_DECLARE_METATYPE(std::shared_ptr<QOpenGLFramebufferObject>)
Q_DECLARE_METATYPE(std::shared_ptr<AVFrame>)
Q_DECLARE_METATYPE(std::shared_ptr<AVPacket>)

#endif//LIBAVQT_GLOBAL_H
