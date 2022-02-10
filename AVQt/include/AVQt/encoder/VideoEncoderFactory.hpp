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

#ifndef LIBAVQT_VIDEOENCODERFACTORY_HPP
#define LIBAVQT_VIDEOENCODERFACTORY_HPP

#include "IVideoEncoderImpl.hpp"
#include "AVQt/encoder/VideoEncoder.hpp"

#include <static_block.hpp>

#include <QMap>
#include <QString>


namespace AVQt {
    class VideoEncoderFactory {
    public:
        static VideoEncoderFactory &getInstance();

        bool registerEncoder(const api::VideoEncoderInfo &info);

        void unregisterEncoder(const QString &name);
        void unregisterEncoder(const api::VideoEncoderInfo &info);

        [[nodiscard]] std::shared_ptr<api::IVideoEncoderImpl> create(const common::PixelFormat &inputFormat, AVCodecID codec, const EncodeParameters &encodeParams, const QStringList &priority = {});

        static void registerEncoders();

    private:
        VideoEncoderFactory() = default;
        QList<api::VideoEncoderInfo> m_encoders;
    };
}// namespace AVQt

//static_block {
//    AVQt::VideoEncoderFactory::registerEncoders();
//};

#endif//LIBAVQT_VIDEOENCODERFACTORY_HPP
