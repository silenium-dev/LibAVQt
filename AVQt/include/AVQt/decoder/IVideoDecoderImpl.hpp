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


#ifndef LIBAVQT_IVIDEODECODERIMPL_HPP
#define LIBAVQT_IVIDEODECODERIMPL_HPP

#include "AVQt/common/PixelFormat.hpp"
#include "AVQt/common/Platform.hpp"
#include "AVQt/communication/VideoPadParams.hpp"
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace AVQt::api {
    class IVideoDecoderImpl {
    public:
        virtual ~IVideoDecoderImpl() = default;

        virtual bool open(std::shared_ptr<AVCodecParameters> codecParams) = 0;
        virtual void close() = 0;

        virtual int decode(std::shared_ptr<AVPacket> packet) = 0;

        [[nodiscard]] virtual AVPixelFormat getOutputFormat() const = 0;
        [[nodiscard]] virtual AVPixelFormat getSwOutputFormat() const {// Defaults to getOutputFormat(), but can be overridden for HW decoding
            return getOutputFormat();
        };
        [[nodiscard]] virtual bool isHWAccel() const = 0;
        [[nodiscard]] virtual communication::VideoPadParams getVideoParams() const = 0;

    signals:
        virtual void frameReady(std::shared_ptr<AVFrame> frame) = 0;
    };

    struct VideoDecoderInfo {
        QMetaObject metaObject;
        QString name;
        QList<common::Platform::Platform_t> platforms;
        QList<common::PixelFormat> supportedInputPixelFormats;
        QList<AVCodecID> supportedCodecIds;
        [[nodiscard]] common::PixelFormat outputPixelFormatFor(const common::PixelFormat &inputPixelFormat) const {
            if (supportedInputPixelFormats.contains(inputPixelFormat)) {
                return inputPixelFormat.toNativeFormat();
            }
            return {};// No conversion possible
        }
    };
}// namespace AVQt::api

Q_DECLARE_INTERFACE(AVQt::api::IVideoDecoderImpl, "AVQt.api.IVideoDecoderImpl")


#endif//LIBAVQT_IVIDEODECODERIMPL_HPP
