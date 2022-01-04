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
// Created by silas on 28.12.21.
//

#ifndef LIBAVQT_IENCODERIMPL_HPP
#define LIBAVQT_IENCODERIMPL_HPP

#include "communication/VideoPadParams.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <QObject>
#include <QVector>

namespace AVQt {
    enum class Codec {
        H264,
        HEVC,
        VP8,
        VP9,
        MPEG2
    };
    struct EncodeParameters {
        int32_t bitrate;
        int32_t max_bitrate;
        int32_t min_bitrate;

        Codec codec;
    };
    namespace api {
        class IEncoderImpl {
        public:
            explicit IEncoderImpl(const EncodeParameters &parameters);
            virtual ~IEncoderImpl() = default;

            virtual bool open(const VideoPadParams &params) = 0;
            virtual void close() = 0;

            virtual int encode(AVFrame *frame) = 0;
            [[nodiscard]] virtual AVPacket *nextPacket() = 0;

            [[nodiscard]] virtual QVector<AVPixelFormat> getInputFormats() const = 0;
            [[nodiscard]] virtual EncodeParameters getEncodeParameters() const;
            [[nodiscard]] virtual bool isHWAccel() const = 0;
            [[nodiscard]] virtual AVCodecParameters *getCodecParameters() const = 0;

        private:
            EncodeParameters m_encodeParameters;
        };
    }// namespace api
}// namespace AVQt

Q_DECLARE_INTERFACE(AVQt::api::IEncoderImpl, "AVQt.IEncoderImpl")


#endif//LIBAVQT_IENCODERIMPL_HPP
