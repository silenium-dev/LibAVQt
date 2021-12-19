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
// Created by silas on 12.12.21.
//

#ifndef LIBAVQT_VAAPIDECODERIMPL_P_HPP
#define LIBAVQT_VAAPIDECODERIMPL_P_HPP

#include <QtGlobal>
#include <QQueue>
#include <QThread>
#include <QMutex>
#include "decoder/VAAPIDecoderImpl.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace AVQt {
    class VAAPIDecoderImplPrivate {
        Q_DECLARE_PUBLIC(VAAPIDecoderImpl)
    private:
        explicit VAAPIDecoderImplPrivate(VAAPIDecoderImpl *q): q_ptr(q) {}

        static AVPixelFormat getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts);

        class FrameFetcher : public QThread {
        public:
            explicit FrameFetcher(VAAPIDecoderImplPrivate *p);

            void run() override;

            void stop();

            AVFrame *nextFrame();

            [[nodiscard]] bool isEmpty() const;

        private:
            VAAPIDecoderImplPrivate *p;
            QQueue<AVFrame *> m_outputQueue;
            QMutex m_mutex;
//            QWaitCondition m_frameAvailable;
            std::atomic_bool m_stop{false};
        };

        VAAPIDecoderImpl *q_ptr;

        AVCodecParameters *codecParams{nullptr};
        AVCodecContext *codecContext{nullptr};
        AVCodec *codec{nullptr};

        AVBufferRef *hwDeviceContext{nullptr};

        FrameFetcher *frameFetcher{nullptr};

//        QWaitCondition frameAvailable;
        QMutex codecMutex;
        std::atomic_bool initialized{false}, firstFrame{true};

        friend class VAAPIDecoderImpl;
    };
}


#endif//LIBAVQT_VAAPIDECODERIMPL_P_HPP
