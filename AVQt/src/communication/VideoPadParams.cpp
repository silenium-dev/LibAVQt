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
// Created by silas on 11.12.21.
//

#include "AVQt/communication/VideoPadParams.hpp"
#include <QtDebug>
#include <boost/uuid/uuid_generators.hpp>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace AVQt::communication {
    const boost::uuids::uuid VideoPadParams::Type = boost::uuids::string_generator()("{1e8b93086ba59290a4ce4fec5b85bd80}");

    VideoPadParams::VideoPadParams(const VideoPadParams &other) : PadUserData(other) {
        *this = other;
    }

    boost::uuids::uuid VideoPadParams::getType() const {
        return Type;
    }

    bool VideoPadParams::isEmpty() const {
        return false;
    }

    boost::json::object VideoPadParams::toJSON() const {
        return PadUserData::toJSON();
    }

    VideoPadParams &VideoPadParams::operator=(const VideoPadParams &other) {
        if (this == &other) {
            return *this;
        }
        frameSize = other.frameSize;
        pixelFormat = other.pixelFormat;
        swPixelFormat = other.swPixelFormat;
        isHWAccel = other.isHWAccel;
        hwFramesContext = other.hwFramesContext;
        hwDeviceContext = other.hwDeviceContext;
        return *this;
    }
}// namespace AVQt::communication
