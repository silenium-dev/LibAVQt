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
// Created by silas on 23.11.21.
//

#include "PacketPadParams.h"
#include <boost/json.hpp>

const boost::uuids::uuid AVQt::PacketPadParams::Type = boost::uuids::string_generator()("{03de5c18bf2d1d7ac97e42cb89121a95}");

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
    data["codec"] = codec;
    data["streamIndex"] = streamIdx;
    obj["data"] = data;
    return obj;
}
