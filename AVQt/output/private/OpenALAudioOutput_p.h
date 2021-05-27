//
// Created by silas on 3/28/21.
//

#include "../RenderClock.h"

extern "C" {
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
}

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <QtCore>

#ifndef LIBAVQT_OPENALAUDIOOUTPUT_P_H
#define LIBAVQT_OPENALAUDIOOUTPUT_P_H

namespace AVQt {
    class OpenALAudioOutput;

    class OpenALAudioOutputPrivate {
    public:
        OpenALAudioOutputPrivate(const OpenALAudioOutputPrivate &) = delete;

        void operator=(const OpenALAudioOutputPrivate &) = delete;

    private:
        explicit OpenALAudioOutputPrivate(OpenALAudioOutput *q) : q_ptr(q) {};

        OpenALAudioOutput *q_ptr;

        QMutex m_inputQueueMutex{};
        QQueue<QPair<AVFrame *, int64_t>> m_inputQueue{};
//        AVFrame *m_partialFrame {nullptr};
//        size_t m_partialFrameOffset {0};
        QMutex m_outputQueueMutex{};
        QQueue<QPair<AVFrame *, int64_t>> m_outputQueue{};
        std::atomic_bool m_outputSliceDurationChanged{false};
        int64_t m_duration{0}, m_clockInterval{0}, m_sampleRate{0};

        std::atomic<RenderClock *> m_clock{nullptr};

        QMutex m_swrContextMutex{};
        SwrContext *m_pSwrContext{nullptr};

        int m_ALBufferCount{5};
        std::atomic_int64_t m_playingBuffers{0};
        QVector<ALuint> m_ALBuffers{};
        QRecursiveMutex m_ALBufferQueueMutex{};
        QQueue<ALuint> m_ALBufferQueue{};
        QRecursiveMutex m_ALBufferSampleMapMutex{};
        QMap<ALuint, int64_t> m_ALBufferSampleMap{};
        int64_t m_queuedSamples{0};
        ALCdevice *m_ALCDevice{nullptr};
        ALCcontext *m_ALCContext{nullptr};

        ALuint m_ALSource{0};
        // Threading stuff
        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};

        std::atomic_bool m_wasPaused{false};
        std::atomic_int64_t m_lastUpdate{0};
        std::atomic_size_t m_audioFrame{0};

        friend class OpenALAudioOutput;
    };
}

#endif //LIBAVQT_OPENALAUDIOOUTPUT_P_H