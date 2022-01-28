// Copyright (c) 2022.
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
// Created by silas on 13.01.22.
//

#ifndef LIBAVQT_ITRANSCODERIMPL_HPP
#define LIBAVQT_ITRANSCODERIMPL_HPP

#include "AVQt/communication/PacketPadParams.hpp"
#include "AVQt/communication/VideoPadParams.hpp"

#include <QObject>

namespace AVQt::api {
    class ITranscoderImpl {
    public:
        virtual ~ITranscoderImpl() = default;

        virtual bool open(PacketPadParams const &params) = 0;
        virtual void close() = 0;

        virtual int transcode(AVPacket *packet) = 0;
        [[nodiscard]] virtual AVPacket *nextPacket() = 0;

        [[nodiscard]] virtual PacketPadParams getPacketOutputParams() const = 0;
        [[nodiscard]] virtual PacketPadParams getPacketOutputParamsForInputParams(PacketPadParams inputParams) const = 0;

        [[nodiscard]] virtual VideoPadParams getVideoOutputParams() const = 0;
        [[nodiscard]] virtual VideoPadParams getVideoOutputParamsForInputParams(PacketPadParams inputParams) const = 0;
    signals:
        virtual void frameReady(std::shared_ptr<AVFrame> frame) = 0;
    };
}// namespace AVQt::api

Q_DECLARE_INTERFACE(AVQt::api::ITranscoderImpl, "AVQt.ITranscoderImpl")

#endif//LIBAVQT_ITRANSCODERIMPL_HPP
