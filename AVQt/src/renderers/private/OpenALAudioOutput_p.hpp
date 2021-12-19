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

#ifndef LIBAVQT_OPENALAUDIOOUTPUT_P_H
#define LIBAVQT_OPENALAUDIOOUTPUT_P_H

#include "renderers/OpenALAudioOutput.hpp"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <QtCore>

extern "C" {
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
}

namespace AVQt {
    class OpenALAudioOutputPrivate : public QObject {
        Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::OpenALAudioOutput)

    private:
        explicit OpenALAudioOutputPrivate(OpenALAudioOutput *q) : q_ptr(q) {}

        OpenALAudioOutput *q_ptr;

        SwrContext *m_pSwrContext{nullptr};
        int m_sampleRate{0};
        uint64_t m_channelLayout{0};
        AVSampleFormat m_sampleFormat{AV_SAMPLE_FMT_NONE};

        QMutex m_inputQueueMutex{};
        QQueue<AVFrame *> m_inputQueue{};
        QMutex m_outputQueueMutex{};
        QQueue<AVFrame *> m_outputQueue{};

        int AL_BUFFER_COUNT = 500;
        static constexpr int AL_BUFFER_ALLOC_STEPS = 50;
        static constexpr size_t OUT_SAMPLE_RATE = 48000;
        static constexpr AVSampleFormat OUT_SAMPLE_FORMAT = AV_SAMPLE_FMT_FLT;
        static constexpr int OUT_CHANNEL_LAYOUT = AV_CH_LAYOUT_STEREO;

        ALCdevice *m_alcDevice{nullptr};
        ALCcontext *m_alcContext{nullptr};
        ALuint m_alSource{0};

        //        std::atomic_uint_fast64_t m_queuedBuffers{0};
        //        QMutex m_alBufferMutex{};
        QVector<ALuint> m_alBuffers{};
        QQueue<ALuint> m_alBufferQueue{};

        std::atomic_bool m_running{false};
        //        std::atomic_bool m_open{false};

        //        std::atomic_bool m_wasPaused{false};
        //        std::atomic_int64_t m_lastUpdate{0};
        //        std::atomic_size_t m_audioFrame{0};
        std::atomic_bool m_firstFrame{true};
        std::atomic<IAudioSource *> m_sourceLock{nullptr};

        friend class OpenALAudioOutput;
    };
}

#endif //LIBAVQT_OPENALAUDIOOUTPUT_P_H
