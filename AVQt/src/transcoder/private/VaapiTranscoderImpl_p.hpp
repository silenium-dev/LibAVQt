// Copyright (c) 2022.
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
// Created by silas on 13.01.22.
//

#ifndef LIBAVQT_VAAPITRANSCODERIMPL_P_HPP
#define LIBAVQT_VAAPITRANSCODERIMPL_P_HPP

#include "communication/FrameDestructor.hpp"
#include "global.hpp"

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

namespace AVQt {
    class VAAPITranscoderImpl;
    class VAAPITranscoderImplPrivate {
        Q_DECLARE_PUBLIC(VAAPITranscoderImpl)
    private:
        class PacketFetcher : public QThread {
        protected:
            void run() override;

        public:
            explicit PacketFetcher(VAAPITranscoderImplPrivate *p);

        private:
            VAAPITranscoderImplPrivate *p;
        };

        explicit VAAPITranscoderImplPrivate(VAAPITranscoderImpl *q) : q_ptr(q) {}

        static AVCodec *getDecoder(Codec codec);
        static AVCodec *getDecoder(AVCodecID codec);
        static AVCodec *getEncoder(Codec codec);
        static AVCodec *getEncoder(AVCodecID codec);
        static AVPixelFormat getNativePixelFormat(AVPixelFormat format);
        static AVPixelFormat getNativePixelFormat(int format);

        static AVPixelFormat getFormat(AVCodecContext *ctx, const AVPixelFormat *pixelFormats);

        VAAPITranscoderImpl *q_ptr;

        EncodeParameters encodeParameters;
        PacketPadParams inputParams;

        AVCodec *encoder = nullptr;
        AVCodecContext *encodeContext = nullptr;
        AVCodec *decoder = nullptr;
        AVCodecContext *decodeContext = nullptr;

        AVBufferRef *hwDeviceContext = nullptr;
        AVBufferRef *hwFramesContext = nullptr;

        QQueue<AVPacket *> outputQueue;
        std::unique_ptr<PacketFetcher> packetFetcher{};
        std::shared_ptr<internal::FrameDestructor> frameDestructor{};

        QMutex encoderMutex{}, outputQueueMutex{};
        std::atomic_bool initialized{false};
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPITRANSCODERIMPL_P_HPP
