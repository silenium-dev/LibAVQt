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

/*!
 * \private
 * \internal
 */

#include "decoder/DecoderQSV.hpp"

extern "C" {
#include <libavutil/rational.h>
}


#ifndef TRANSCODE_DECODERQSV_P_H
#define TRANSCODE_DECODERQSV_P_H

namespace AVQt {
    /*!
     * \private
     */
    class DecoderQSVPrivate : public QObject {
    Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::DecoderQSV)

    public:
        DecoderQSVPrivate(const DecoderQSVPrivate &) = delete;

        void operator=(const DecoderQSVPrivate &) = delete;

    private:
        explicit DecoderQSVPrivate(DecoderQSV *q) : q_ptr(q) {};

        DecoderQSV *q_ptr;

        QMutex m_inputQueueMutex{};
        QQueue<AVPacket *> m_inputQueue{};
        int64_t m_duration{0};
        AVRational m_framerate{};
        AVRational m_timebase{};

        AVCodec *m_pCodec{nullptr};
        AVCodecParameters *m_pCodecParams{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVBufferRef *m_pDeviceCtx{nullptr};

        // Callback stuff
        QMutex m_cbListMutex{};
        QList<IFrameSink *> m_cbList{};

        // Threading stuff
        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};

        static AVPixelFormat getFormat(AVCodecContext *pCodecCtx, const enum AVPixelFormat *pix_fmts);

        friend class DecoderQSV;
    };
}

#endif //TRANSCODE_DECODERQSV_P_H
