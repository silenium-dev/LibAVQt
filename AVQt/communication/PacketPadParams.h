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
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BELIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORTOR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OROTHER DEALINGS IN THE SOFTWARE.

//
// Created by silas on 29.10.21.
//

#ifndef LIBAVQT_PACKETPADPARAMS_H
#define LIBAVQT_PACKETPADPARAMS_H

#include <QMetaType>

extern "C" {
#include "libavcodec/avcodec.h"
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
}

namespace AVQt {
    struct PacketPadParams {
        AVMediaType mediaType{AVMEDIA_TYPE_UNKNOWN};
        AVCodecID codec{AV_CODEC_ID_NONE};
        int64_t stream{};
    };
}// namespace AVQt

Q_DECLARE_METATYPE(AVQt::PacketPadParams);

#endif//LIBAVQT_PACKETPADPARAMS_H
