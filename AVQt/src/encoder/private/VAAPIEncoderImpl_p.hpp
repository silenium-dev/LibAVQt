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

#include "encoder/VAAPIEncoderImpl.hpp"

#include <QtCore>

namespace AVQt {
    class VAAPIEncoderImpl;
    class VAAPIEncoderImplPrivate {
        Q_DECLARE_PUBLIC(VAAPIEncoderImpl)
    private:
        explicit VAAPIEncoderImplPrivate(VAAPIEncoderImpl *q) : q_ptr(q) {}
        VAAPIEncoderImpl *q_ptr;

        int allocateHwFrame();

        class PacketFetcher : public QThread {
        public:
            explicit PacketFetcher(VAAPIEncoderImplPrivate *p);

            void run() override;

            void stop();

            AVPacket *nextPacket();

            [[nodiscard]] bool isEmpty() const;

        private:
            VAAPIEncoderImplPrivate *p;
            QQueue<AVPacket *> m_outputQueue;
            QMutex m_mutex;
            //            QWaitCondition m_frameAvailable;
            std::atomic_bool m_stop{false};
        };
        EncodeParameters encodeParameters{};

        AVCodecParameters *codecParams{nullptr};
        AVCodecContext *codecContext{nullptr};
        AVCodec *codec{nullptr};

        AVBufferRef *hwDeviceContext{nullptr};
        AVBufferRef *hwFramesContext{nullptr};
        AVFrame *hwFrame{nullptr};
        std::atomic_bool mappable{true};

        PacketFetcher *packetFetcher{nullptr};

        QMutex codecMutex;
        std::atomic_bool initialized{false}, firstFrame{true};
        std::atomic_uint64_t frameCounter{0};
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPIENCODERIMPL_P_HPP
