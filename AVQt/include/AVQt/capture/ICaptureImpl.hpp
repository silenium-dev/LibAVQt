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
// Created by silas on 25.01.22.
//

#ifndef LIBAVQT_ICAPTUREIMPL_HPP
#define LIBAVQT_ICAPTUREIMPL_HPP

#include "communication/VideoPadParams.hpp"

#include <QObject>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace AVQt::api {
    class ICaptureImpl {
    public:
        enum class SourceClass {
            Window,
            Screen,
            Any
        };
        virtual ~ICaptureImpl() = default;

        virtual void open(const SourceClass &sourceClass) = 0;
        virtual void close() = 0;

        virtual std::unique_ptr<AVFrame> nextFrame() = 0;

        [[nodiscard]] virtual AVPixelFormat getOutputFormat() const = 0;
        [[nodiscard]] virtual AVPixelFormat getSwOutputFormat() const {// Defaults to getOutputFormat(), can be overridden for HW accelerated formats
            return getOutputFormat();
        }

        [[nodiscard]] virtual bool isHWAccel() const = 0;
        [[nodiscard]] virtual VideoPadParams getVideoParams() const = 0;
    };
}// namespace AVQt::api

Q_DECLARE_INTERFACE(AVQt::api::ICaptureImpl, "AVQt.api.ICaptureImpl")

#endif//LIBAVQT_ICAPTUREIMPL_HPP
