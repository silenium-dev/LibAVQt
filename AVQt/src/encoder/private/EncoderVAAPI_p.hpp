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

#include "encoder/EncoderVAAPI.hpp"

#include <QtCore>

#ifndef LIBAVQT_ENCODERVAAPI_P_H
#define LIBAVQT_ENCODERVAAPI_P_H

namespace AVQt {
    class EncoderVAAPIPrivate {
    public:
        EncoderVAAPIPrivate(const EncoderVAAPIPrivate &) = delete;

        void operator=(const EncoderVAAPIPrivate &) = delete;

    private:
        explicit EncoderVAAPIPrivate(EncoderVAAPI *q) : q_ptr(q) {};

        EncoderVAAPI *q_ptr;

        IEncoder::CODEC m_codec{IEncoder::CODEC::H264};
        int m_bitrate{5 * 1024 * 1024};
        qint64 m_duration{};

        AVCodec *m_pCodec{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVBufferRef *m_pDeviceCtx{nullptr}, *m_pFramesCtx{nullptr};
        AVFrame *m_pHWFrame{nullptr};

        QMutex m_inputQueueMutex{};
        QQueue<QPair<AVFrame *, int64_t>> m_inputQueue{};

        QMutex m_cbListMutex{};
        QList<IPacketSink *> m_cbList{};

        std::atomic_bool m_running{false}, m_paused{false}, m_firstFrame{true};

        friend class EncoderVAAPI;
    };
}

#endif //LIBAVQT_ENCODERVAAPI_P_H
