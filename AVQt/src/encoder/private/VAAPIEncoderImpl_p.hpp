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

#ifndef LIBAVQT_VAAPIENCODERIMPL_P_HPP
#define LIBAVQT_VAAPIENCODERIMPL_P_HPP

#include "communication/PacketDestructor.hpp"
#include "encoder/VAAPIEncoderImpl.hpp"

#include <QtCore>
#include <queue>


namespace AVQt {
    class VAAPIEncoderImpl;
    namespace internal {
        class PacketFetcher;
    }

    class VAAPIEncoderImplPrivate {
        Q_DECLARE_PUBLIC(VAAPIEncoderImpl)
    public:
        static void destroyAVBufferRef(AVBufferRef *buffer);

        static void destroyAVCodecContext(AVCodecContext *codecContext);
        static void destroyAVCodecParameters(AVCodecParameters *codecParameters);

        static void destroyAVFrame(AVFrame *frame);

    private:
        explicit VAAPIEncoderImplPrivate(VAAPIEncoderImpl *q) : q_ptr(q) {}
        VAAPIEncoderImpl *q_ptr;

        int mapFrameToHW(std::shared_ptr<AVFrame> &output, const std::shared_ptr<AVFrame> &frame);

        int allocateHWFrame(std::shared_ptr<AVFrame> &output);

        EncodeParameters encodeParameters{};

        std::shared_ptr<AVCodecParameters> codecParams{};
        std::unique_ptr<AVCodecContext, decltype(&destroyAVCodecContext)> codecContext{nullptr, &destroyAVCodecContext};
        AVCodec *codec{nullptr};

        std::shared_ptr<AVBufferRef> hwDeviceContext{};
        std::shared_ptr<AVBufferRef> hwFramesContext{};
        std::shared_ptr<AVFrame> hwFrame{};
        std::atomic_bool derivedContext{false};

        std::unique_ptr<internal::PacketFetcher> packetFetcher{};

        QMutex codecMutex;
        std::atomic_bool initialized{false}, firstFrame{true};

        const static QList<AVPixelFormat> supportedPixelFormats;

        friend class internal::PacketFetcher;
    };
    namespace internal {
        class PacketFetcher : public QThread {
            Q_OBJECT
        public:
            explicit PacketFetcher(VAAPIEncoderImplPrivate *p);

            void run() override;

            void stop();

        private:
            VAAPIEncoderImplPrivate *p;
            bool m_stop{false};
        };
    }// namespace internal
}// namespace AVQt


#endif//LIBAVQT_VAAPIENCODERIMPL_P_HPP
