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



#ifndef LIBAVQT_VIDEOPADPARAMS_HPP
#define LIBAVQT_VIDEOPADPARAMS_HPP

#include "pgraph/api/PadUserData.hpp"
#include <QtCore/QObject>
#include <QtCore/QSize>

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/pixfmt.h>
}

namespace AVQt::communication {
    class VideoPadParams : public pgraph::api::PadUserData {
    public:
        explicit VideoPadParams() = default;
        VideoPadParams(const VideoPadParams &other) = default;
        VideoPadParams &operator=(const VideoPadParams &other) = default;

        ~VideoPadParams() override = default;
        QUuid getType() const override;
        QJsonObject toJSON() const override;

    protected:
        bool isEmpty() const override;

    public:
        static const QUuid Type;

        QSize frameSize{};
        AVPixelFormat pixelFormat{}, swPixelFormat{};
        bool isHWAccel{false};
        std::shared_ptr<AVBufferRef> hwDeviceContext{nullptr};
        std::shared_ptr<AVBufferRef> hwFramesContext{nullptr};
    };
}// namespace AVQt::communication

Q_DECLARE_METATYPE(AVQt::communication::VideoPadParams)
Q_DECLARE_METATYPE(std::shared_ptr<AVQt::communication::VideoPadParams>)
Q_DECLARE_METATYPE(std::shared_ptr<const AVQt::communication::VideoPadParams>)

#endif//LIBAVQT_VIDEOPADPARAMS_HPP
