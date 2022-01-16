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
// Created by silas on 23.11.21.
//

#include "communication/PacketPadParams.hpp"
#include <boost/json.hpp>

const boost::uuids::uuid AVQt::PacketPadParams::Type = boost::uuids::string_generator()("{03de5c18bf2d1d7ac97e42cb89121a95}");

AVQt::PacketPadParams::PacketPadParams(const AVQt::PacketPadParams &other) : PadUserData(other) {
    *this = other;
}

AVQt::PacketPadParams &AVQt::PacketPadParams::operator=(const AVQt::PacketPadParams &other) {
    if (&other == this) {
        return *this;
    }

    mediaType = other.mediaType;
    codec = other.codec;
    streamIdx = other.streamIdx;
    codecParams = avcodec_parameters_alloc();
    avcodec_parameters_copy(codecParams, other.codecParams);
    return *this;
}

AVQt::PacketPadParams::~PacketPadParams() {
    if (codecParams) {
        avcodec_parameters_free(&codecParams);
    }
}
AVQt::PacketPadParams::PacketPadParams(AVQt::PacketPadParams &&other) noexcept {
    *this = other;
    avcodec_parameters_free(&other.codecParams);
}
boost::uuids::uuid AVQt::PacketPadParams::getType() const {
    return Type;
}
bool AVQt::PacketPadParams::isEmpty() const {
    return false;
}
boost::json::object AVQt::PacketPadParams::toJSON() const {
    boost::json::object obj, data;
    obj["type"] = boost::uuids::hash_value(Type);
    data["mediaType"] = mediaType;
    data["encoder"] = codec;
    data["streamIndex"] = streamIdx;
    obj["data"] = data;
    return obj;
}
