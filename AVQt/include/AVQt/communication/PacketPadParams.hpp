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
// Created by silas on 29.10.21.
//

#ifndef LIBAVQT_PACKETPADPARAMS_H
#define LIBAVQT_PACKETPADPARAMS_H

#include <QMetaType>
#include <pgraph/api/PadUserData.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
}

namespace AVQt::communication {
    class PacketPadParams : public pgraph::api::PadUserData {
    public:
        explicit PacketPadParams() = default;
        PacketPadParams(const PacketPadParams &);
        PacketPadParams &operator=(const PacketPadParams &);
        PacketPadParams(PacketPadParams &&) noexcept;
        ~PacketPadParams() override = default;

        QUuid getType() const override;
        QJsonObject toJSON() const override;

        static const QUuid Type;

    protected:
        bool isEmpty() const override;

    public:
        AVMediaType mediaType{AVMEDIA_TYPE_UNKNOWN};
        const AVCodec *codec{nullptr};
        std::shared_ptr<AVCodecParameters> codecParams{};
        int64_t streamIdx{};
    };
}// namespace AVQt::communication

Q_DECLARE_METATYPE(AVQt::communication::PacketPadParams);
Q_DECLARE_METATYPE(std::shared_ptr<AVQt::communication::PacketPadParams>)
Q_DECLARE_METATYPE(std::shared_ptr<const AVQt::communication::PacketPadParams>)

#endif//LIBAVQT_PACKETPADPARAMS_H
