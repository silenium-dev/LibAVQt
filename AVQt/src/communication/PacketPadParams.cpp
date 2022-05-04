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


#include "AVQt/communication/PacketPadParams.hpp"

namespace AVQt::communication {
    const QUuid PacketPadParams::Type = QUuid::fromString(QLatin1String{"03de5c18bf2d1d7ac97e42cb89121a95"});

    PacketPadParams::PacketPadParams(const PacketPadParams &other) : PadUserData(other) {
        *this = other;
    }

    PacketPadParams &PacketPadParams::operator=(const PacketPadParams &other) {
        if (&other == this) {
            return *this;
        }

        mediaType = other.mediaType;
        codec = other.codec;
        streamIdx = other.streamIdx;
        codecParams = other.codecParams;
        return *this;
    }

    PacketPadParams::PacketPadParams(PacketPadParams &&other) noexcept {
        *this = other;
    }

    QUuid PacketPadParams::getType() const {
        return Type;
    }

    bool PacketPadParams::isEmpty() const {
        return false;
    }

    QJsonObject PacketPadParams::toJSON() const {
        QJsonObject obj, data;
        obj["type"] = Type.toString();
        data["mediaType"] = mediaType;
        data["encoder"] = codec->name;
        data["streamIndex"] = static_cast<qint64>(streamIdx);
        obj["data"] = data;
        return obj;
    }
}// namespace AVQt::communication
