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
