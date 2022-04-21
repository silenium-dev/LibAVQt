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

#ifndef LIBAVQT_IVIDEOENCODERIMPL_HPP
#define LIBAVQT_IVIDEOENCODERIMPL_HPP

#include "AVQt/common/PixelFormat.hpp"
#include "AVQt/common/Platform.hpp"
#include "AVQt/communication/PacketPadParams.hpp"
#include "AVQt/communication/VideoPadParams.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <QtCore/QObject>
#include <QtCore/QVector>

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
    };
    namespace api {
        class IVideoEncoderImpl {
        public:
            explicit IVideoEncoderImpl(const EncodeParameters &parameters);
            virtual ~IVideoEncoderImpl() = default;

            [[nodiscard]] virtual EncodeParameters getEncodeParameters() const;

            virtual bool open(const communication::VideoPadParams &params) = 0;
            virtual void close() = 0;

            [[nodiscard]] virtual std::shared_ptr<AVFrame> prepareFrame(std::shared_ptr<AVFrame> frame) = 0;
            virtual int encode(std::shared_ptr<AVFrame> frame) = 0;

            [[nodiscard]] virtual bool isHWAccel() const = 0;

            [[nodiscard]] virtual QVector<AVPixelFormat> getInputFormats() const = 0;
            [[nodiscard]] virtual std::shared_ptr<AVCodecParameters> getCodecParameters() const = 0;
            [[nodiscard]] virtual std::shared_ptr<communication::PacketPadParams> getPacketPadParams() const = 0;
        signals:
            virtual void packetReady(std::shared_ptr<AVPacket> packet) = 0;

        private:
            EncodeParameters m_encodeParameters;
        };

        struct VideoEncoderInfo {
            QMetaObject metaObject;
            QString name;
            QList<common::Platform::Platform_t> platforms;
            QList<common::PixelFormat> supportedInputPixelFormats;
            QList<AVCodecID> supportedCodecIds;
        };
    }// namespace api
}// namespace AVQt

Q_DECLARE_INTERFACE(AVQt::api::IVideoEncoderImpl, "AVQt.IVideoEncoderImpl")

Q_DECLARE_METATYPE(AVQt::EncodeParameters)

#endif//LIBAVQT_IVIDEOENCODERIMPL_HPP
