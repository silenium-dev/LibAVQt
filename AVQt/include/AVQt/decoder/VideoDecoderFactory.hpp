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


#ifndef LIBAVQT_VIDEODECODERFACTORY_HPP
#define LIBAVQT_VIDEODECODERFACTORY_HPP

#include "AVQt/decoder/IVideoDecoderImpl.hpp"

#include <QtCore/QMap>

namespace AVQt {
    class VideoDecoderFactory {
        Q_DISABLE_COPY_MOVE(VideoDecoderFactory)
    public:
        static VideoDecoderFactory &getInstance();

        bool registerDecoder(const api::VideoDecoderInfo &info);

        void unregisterDecoder(const QString &name);
        void unregisterDecoder(const api::VideoDecoderInfo &info);

        [[nodiscard]] std::shared_ptr<api::IVideoDecoderImpl> create(const common::PixelFormat &inputFormat, AVCodecID codec, const QStringList &priority = {});

    private:
        VideoDecoderFactory() = default;
        QList<api::VideoDecoderInfo> m_decoders;
    };
}// namespace AVQt

#endif//LIBAVQT_VIDEODECODERFACTORY_HPP
