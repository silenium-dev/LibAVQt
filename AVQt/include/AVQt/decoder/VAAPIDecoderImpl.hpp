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
// Created by silas on 12.12.21.
//

#ifndef LIBAVQT_VAAPIDECODERIMPL_HPP
#define LIBAVQT_VAAPIDECODERIMPL_HPP

#include "IDecoderImpl.hpp"
#include <static_block.hpp>

namespace AVQt {
    class VAAPIDecoderImplPrivate;

    class VAAPIDecoderImpl : public QObject, public AVQt::api::IDecoderImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(VAAPIDecoderImpl)
        Q_INTERFACES(AVQt::api::IDecoderImpl)
    public:
        Q_INVOKABLE explicit VAAPIDecoderImpl();

        ~VAAPIDecoderImpl() override = default;

        bool open(AVCodecParameters *codecParams) override;
        void close() override;
        int decode(AVPacket *packet) override;
        AVFrame *nextFrame() override;

        [[nodiscard]] AVPixelFormat getOutputFormat() const override;
        [[nodiscard]] bool isHWAccel() const override;

        [[nodiscard]] AVRational getTimeBase() const override;

    protected:
        VAAPIDecoderImplPrivate *d_ptr;
    };
}// namespace AVQt

#endif//LIBAVQT_VAAPIDECODERIMPL_HPP
