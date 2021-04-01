//
// Created by silas on 3/28/21.
//

#include "../OpenALAudioOutput.h"
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
    class OpenALAudioOutputPrivate {
        explicit OpenALAudioOutputPrivate(OpenALAudioOutput *q) : q_ptr(q) {};

        OpenALAudioOutput *q_ptr;

        QMutex m_inputQueueMutex;
        QQueue<QPair<AVFrame *, int64_t>> m_inputQueue;
        QMutex m_outputQueueMutex;
        QQueue<QPair<AVFrame *, int64_t>> m_outputQueue;
        std::atomic_bool m_outputSliceDurationChanged = false;
        int64_t m_duration = 0;

        RenderClock *m_clock = nullptr;

        QMutex m_swrContextMutex;
        SwrContext *m_pSwrContext = nullptr;

        static constexpr size_t BUFFER_COUNT = 100;
        ALuint m_ALBuffers[BUFFER_COUNT];
        ALCdevice *m_ALCDevice = nullptr;
        ALCcontext *m_ALCContext = nullptr;
        ALuint m_ALSource = 0;

        // Threading stuff
        std::atomic_bool m_running = false;
        std::atomic_bool m_paused = false;
        std::atomic_bool m_firstFrame = true;

        friend class OpenALAudioOutput;
    };
}

#endif //LIBAVQT_OPENALAUDIOOUTPUT_P_H
