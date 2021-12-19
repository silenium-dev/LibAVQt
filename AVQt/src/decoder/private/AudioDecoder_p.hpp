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

#include "decoder/AudioDecoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <QtCore>

#ifndef LIBAVQT_AUDIODECODER_P_H
#define LIBAVQT_AUDIODECODER_P_H

namespace AVQt {
    class AudioDecoderPrivate : public QObject {
    Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::AudioDecoder)

    public:
        AudioDecoderPrivate(const AudioDecoderPrivate &) = delete;

        void operator=(const AudioDecoderPrivate &) = delete;

    private:
        explicit AudioDecoderPrivate(AudioDecoder *q) : q_ptr(q) {};

        AudioDecoder *q_ptr;

        QMutex m_inputQueueMutex{};
        QQueue<AVPacket *> m_inputQueue{};
        int64_t m_duration{0};

        AVCodecParameters *m_pCodecParams{nullptr};
        AVCodec *m_pCodec{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVRational m_timebase{0, 1};

        // Callback stuff
        QMutex m_cbListMutex{};
        QList<IAudioSink *> m_cbList{};

        // Threading stuff
        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};

        friend class AudioDecoder;
    };
}

#endif //LIBAVQT_AUDIODECODER_P_H
