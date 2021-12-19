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

//
// Created by silas on 12.12.21.
//

#ifndef LIBAVQT_IDECODERIMPL_HPP
#define LIBAVQT_IDECODERIMPL_HPP

#include <QObject>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace AVQt::api {
    class IDecoderImpl {
    public:
        virtual ~IDecoderImpl() = default;

        virtual bool open(AVCodecParameters *codecParams) = 0;
        virtual void close() = 0;

        virtual int decode(AVPacket *packet) = 0;
        virtual AVFrame *nextFrame() = 0;

        [[nodiscard]] virtual AVPixelFormat getOutputFormat() const = 0;
        [[nodiscard]] virtual bool isHWAccel() const = 0;
        [[nodiscard]] virtual AVRational getTimeBase() const = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::api::IDecoderImpl, "AVQt.api.IDecoderImpl")


#endif//LIBAVQT_IDECODERIMPL_HPP
