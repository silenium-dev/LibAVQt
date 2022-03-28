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

#ifndef LIBAVQT_VAAPIENCODERIMPL_HPP
#define LIBAVQT_VAAPIENCODERIMPL_HPP

#include "AVQt/encoder/IVideoEncoderImpl.hpp"

#include "AVQt/communication/VideoPadParams.hpp"
#include "AVQt/communication/PacketPadParams.hpp"
#include <QObject>


namespace AVQt {
    class VAAPIEncoderImplPrivate;
    class VAAPIEncoderImpl : public QObject, public api::IVideoEncoderImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(VAAPIEncoderImpl)
        Q_INTERFACES(AVQt::api::IVideoEncoderImpl)
    public:
        static const api::VideoEncoderInfo &info();

        Q_INVOKABLE VAAPIEncoderImpl(AVCodecID codec, AVQt::EncodeParameters parameters);
        ~VAAPIEncoderImpl() override;

        bool open(const communication::VideoPadParams &params) override;
        void close() override;

        [[nodiscard]] std::shared_ptr<AVFrame> prepareFrame(std::shared_ptr<AVFrame> frame) override;
        int encode(std::shared_ptr<AVFrame> frame) override;

        [[nodiscard]] bool isHWAccel() const override;

        [[nodiscard]] QVector<AVPixelFormat> getInputFormats() const override;
        [[nodiscard]] std::shared_ptr<AVCodecParameters> getCodecParameters() const override;

        [[nodiscard]] std::shared_ptr<communication::PacketPadParams> getPacketPadParams() const override;
    signals:
        void packetReady(std::shared_ptr<AVPacket> packet) override;

    private:
        std::unique_ptr<VAAPIEncoderImplPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPIENCODERIMPL_HPP
