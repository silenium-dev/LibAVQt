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

#include "encoder/EncoderQSV.hpp"

#include <QtCore>

#ifndef LIBAVQT_ENCODERQSV_P_H
#define LIBAVQT_ENCODERQSV_P_H

namespace AVQt {
    class EncoderQSVPrivate {
    public:
        EncoderQSVPrivate(const EncoderQSVPrivate &) = delete;

        void operator=(const EncoderQSVPrivate &) = delete;

    private:
        explicit EncoderQSVPrivate(EncoderQSV *q) : q_ptr(q) {};

        EncoderQSV *q_ptr;

        IEncoder::CODEC m_codec{IEncoder::CODEC::H264};
        int m_bitrate{5 * 1024 * 1024};

        AVRational m_framerate{0, 1};
        int64_t m_duration{0};
        AVCodec *m_pCodec{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVBufferRef *m_pDeviceCtx{nullptr}, *m_pFramesCtx{nullptr};
        static constexpr auto HW_FRAME_POOL_SIZE = 4;
        AVFrame *m_pHWFrame{nullptr};

        QMutex m_inputQueueMutex{};

        QQueue<QPair<AVFrame *, int64_t>> m_inputQueue{};
        IFrameSource *m_pLockedSource = nullptr;
        QMutex m_cbListMutex{};

        QList<IPacketSink *> m_cbList{};

        QMutex m_onFrameMutex{};

        std::atomic_bool m_running{false}, m_paused{false}, m_firstFrame{true}, m_initialized{false}, m_codecBufferFull{false};

        friend class EncoderQSV;
    };
}

#endif //LIBAVQT_ENCODERQSV_P_H
