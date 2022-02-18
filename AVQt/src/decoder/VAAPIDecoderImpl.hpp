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
// Created by silas on 12.12.21.
//

#ifndef LIBAVQT_VAAPIDECODERIMPL_HPP
#define LIBAVQT_VAAPIDECODERIMPL_HPP

#include "AVQt/communication/VideoPadParams.hpp"
#include "include/AVQt/decoder/IVideoDecoderImpl.hpp"
#include "static_block.hpp"

namespace AVQt {
    class VAAPIDecoderImplPrivate;

    class VAAPIDecoderImpl : public QObject, public AVQt::api::IVideoDecoderImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(VAAPIDecoderImpl)
        Q_INTERFACES(AVQt::api::IVideoDecoderImpl)
    public:
        static const AVQt::api::VideoDecoderInfo info;

        Q_INVOKABLE explicit VAAPIDecoderImpl(AVCodecID codec);

        ~VAAPIDecoderImpl() override;

        bool open(std::shared_ptr<AVCodecParameters> codecParams) override;
        void close() override;
        int decode(std::shared_ptr<AVPacket> packet) override;

        [[nodiscard]] AVPixelFormat getOutputFormat() const override;
        [[nodiscard]] AVPixelFormat getSwOutputFormat() const override;
        [[nodiscard]] bool isHWAccel() const override;

        [[nodiscard]] communication::VideoPadParams getVideoParams() const override;

    protected:
        VAAPIDecoderImplPrivate *d_ptr;

        [[nodiscard]] static AVPixelFormat getSwOutputFormat(AVPixelFormat format);
        [[nodiscard]] static AVPixelFormat getSwOutputFormat(int format);
    signals:
        void frameReady(std::shared_ptr<AVFrame> frame) override;
    };
}// namespace AVQt

#endif//LIBAVQT_VAAPIDECODERIMPL_HPP
