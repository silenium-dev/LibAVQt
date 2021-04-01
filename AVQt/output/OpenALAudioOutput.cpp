//
// Created by silas on 3/28/21.
//

#include "private/OpenALAudioOutput_p.h"
#include "OpenALAudioOutput.h"

#include "filter/private/OpenALErrorHandler.h"
#include "OpenGLRenderer.h"

namespace AVQt {
    OpenALAudioOutput::OpenALAudioOutput(QObject *parent) : QThread(parent), d_ptr(new OpenALAudioOutputPrivate(this)) {
        Q_D(AVQt::OpenALAudioOutput);

    }

    [[maybe_unused]] OpenALAudioOutput::OpenALAudioOutput(OpenALAudioOutputPrivate &p) : d_ptr(&p) {
        Q_D(AVQt::OpenALAudioOutput);

    }

    int OpenALAudioOutput::init(IAudioSource *source, int64_t duration) {
        Q_D(AVQt::OpenALAudioOutput);
        Q_UNUSED(source);

        d->m_duration = duration;

        d->m_ALCDevice = alcOpenDevice(nullptr);
        if (!d->m_ALCDevice) {
            qFatal("Could not open ALC device");
        }

        if (!alcCall(alcCreateContext, d->m_ALCContext, d->m_ALCDevice, d->m_ALCDevice, nullptr) || !d->m_ALCContext) {
            qFatal("Could not create ALC context");
        }

        ALCboolean contextCurrent = ALC_FALSE;
        if (!alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, d->m_ALCContext) || contextCurrent != ALC_TRUE) {
            qFatal("Could not make ALC context current");
        }

        alCall(alGenBuffers, OpenALAudioOutputPrivate::BUFFER_COUNT, d->m_ALBuffers);

        alCall(alGenSources, 1, &d->m_ALSource);
        alCall(alSourcef, d->m_ALSource, AL_PITCH, 1);
        alCall(alSourcef, d->m_ALSource, AL_GAIN, 1.0f);
        alCall(alSource3f, d->m_ALSource, AL_POSITION, 0, 0, 0);
        alCall(alSource3f, d->m_ALSource, AL_VELOCITY, 0, 0, 0);
        alCall(alSourcei, d->m_ALSource, AL_LOOPING, AL_FALSE);

        constexpr size_t SAMPLE_COUNT = 50;
        uint8_t **data = nullptr;
        int linesize = 0;
        av_samples_alloc_array_and_samples(&data, &linesize, 2, SAMPLE_COUNT, AV_SAMPLE_FMT_S16, 1);
        av_samples_set_silence(data, 0, SAMPLE_COUNT, 2, AV_SAMPLE_FMT_S16);

        for (size_t i = 0; i < 10; ++i) {
            alCall(alBufferData, d->m_ALBuffers[i], AL_FORMAT_STEREO16, &data[0][0],
                   av_samples_get_buffer_size(&linesize, 2, SAMPLE_COUNT, AV_SAMPLE_FMT_S16, 1), 48000);
        }

        av_freep(&data[0]);

        linesize = 0;
        av_samples_alloc_array_and_samples(&data, &linesize, 2, 1, AV_SAMPLE_FMT_S16, 1);
        av_samples_set_silence(data, 0, 1, 2, AV_SAMPLE_FMT_S16);

        for (size_t i = 11; i < OpenALAudioOutputPrivate::BUFFER_COUNT; ++i) {
            alCall(alBufferData, d->m_ALBuffers[i], AL_FORMAT_STEREO16, &data[0][0],
                   av_samples_get_buffer_size(&linesize, 2, 1, AV_SAMPLE_FMT_S16, 1), 48000);
        }

        av_freep(&data[0]);

        alCall(alSourceQueueBuffers, d->m_ALSource, 10, &d->m_ALBuffers[0]);

        alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);

        return 0;
    }

    int OpenALAudioOutput::deinit(IAudioSource *source) {
        Q_D(AVQt::OpenALAudioOutput);

        ALCboolean contextCurrent = ALC_FALSE;
        alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, d->m_ALCContext);

        alCall(alSourcei, d->m_ALSource, AL_SOURCE_STATE, AL_STOPPED);

        alCall(alDeleteSources, 1, &d->m_ALSource);
        alCall(alDeleteBuffers, OpenALAudioOutputPrivate::BUFFER_COUNT, d->m_ALBuffers);

        alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);
        alcCall(alcDestroyContext, d->m_ALCDevice, d->m_ALCContext);

        ALCboolean closed = ALC_FALSE;
        alcCall(alcCloseDevice, closed, d->m_ALCDevice, d->m_ALCDevice);

        return 0;
    }

    int OpenALAudioOutput::start(IAudioSource *source) {
        Q_D(AVQt::OpenALAudioOutput);

        bool shouldBeCurrent = false;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, true)) {
            pause(nullptr, false);
            QThread::start(QThread::TimeCriticalPriority);
            return 0;
        }
        return -1;
    }

    int OpenALAudioOutput::stop(IAudioSource *source) {
        Q_D(AVQt::OpenALAudioOutput);

        bool shouldBeCurrent = true;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, false)) {

            {
                QMutexLocker lock(&d->m_inputQueueMutex);
                for (auto &frame: d->m_inputQueue) {
                    av_frame_unref(frame.first);
                    av_frame_free(&frame.first);
                }
                d->m_inputQueue.clear();
            }

            pause(nullptr, true);
            wait();

            stopped();

            return 0;
        }
        return -1;
    }

    void OpenALAudioOutput::pause(IAudioSource *source, bool pause) {
        Q_D(AVQt::OpenALAudioOutput);

        bool shouldBeCurrent = !pause;
        if (d->m_paused.compare_exchange_strong(shouldBeCurrent, pause)) {
            paused(pause);
        }
    }

    bool OpenALAudioOutput::isPaused() {
        Q_D(AVQt::OpenALAudioOutput);
        return d->m_paused.load();
    }

    void OpenALAudioOutput::onAudioFrame(IAudioSource *source, AVFrame *frame, uint32_t duration) {
        Q_D(AVQt::OpenALAudioOutput);

        QPair<AVFrame *, int64_t> newFrame;

        newFrame.first = av_frame_alloc();
        av_frame_ref(newFrame.first, frame);

        newFrame.second = duration;

        {
            QMutexLocker lock(&d->m_swrContextMutex);
            if (!d->m_pSwrContext) {
                d->m_pSwrContext = swr_alloc_set_opts(nullptr, frame->channel_layout, AV_SAMPLE_FMT_S16, frame->sample_rate,
                                                      frame->channel_layout, static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                                                      0, nullptr);
                swr_init(d->m_pSwrContext);
            }
        }

        while (d->m_inputQueue.size() >= 100) {
            QThread::msleep(4);
        }

        QMutexLocker lock(&d->m_inputQueueMutex);
        d->m_inputQueue.enqueue(newFrame);
    }

    void OpenALAudioOutput::run() {
        Q_D(AVQt::OpenALAudioOutput);
        ALCboolean contextCurrent = ALC_FALSE;
        if (!alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, d->m_ALCContext) || contextCurrent != ALC_TRUE) {
            qFatal("Could not make ALC context current");
        }

        alCall(alSourcePlay, d->m_ALSource);

        uint8_t **cvtData = nullptr;
        int cvtLinesize = 0;

        ALint state = AL_PLAYING;
        while (d->m_running.load()) {
            alCall(alGetSourcei, d->m_ALSource, AL_SOURCE_STATE, &state);
            if (d->m_paused.load() && state == AL_PLAYING) {
                alCall(alSourcePause, d->m_ALSource);
            } else if ((!d->m_paused.load() && state == AL_PAUSED) || state == AL_STOPPED) {
                alCall(alSourcePlay, d->m_ALSource);
            }
            if (!d->m_paused.load()) {
                ALCint buffersProcessed = 0;
                alCall(alGetSourcei, d->m_ALSource, AL_BUFFERS_PROCESSED, &buffersProcessed);
                bool firstFrame = true;
                if (d->m_firstFrame.compare_exchange_strong(firstFrame, false)) {
                    buffersProcessed = OpenALAudioOutputPrivate::BUFFER_COUNT - 11;
                    alCall(alSourceQueueBuffers, d->m_ALSource, buffersProcessed, &d->m_ALBuffers[11]);
                }
                while (buffersProcessed--) {
                    QMutexLocker lock(&d->m_inputQueueMutex);
                    if (!d->m_inputQueue.isEmpty()) {
//                        qDebug("Enqueueing data");
                        auto queueFrame = d->m_inputQueue.dequeue();
                        AVFrame *frame = queueFrame.first;
                        lock.unlock();
                        ALuint buffer;
                        alCall(alSourceUnqueueBuffers, d->m_ALSource, 1, &buffer);
                        AVFrame *outputFrame = av_frame_alloc();
                        if (d->m_pSwrContext) {
                            av_samples_alloc_array_and_samples(&cvtData, &cvtLinesize, frame->channels, frame->nb_samples,
                                                               AV_SAMPLE_FMT_S16, 1);
                            swr_convert(d->m_pSwrContext, cvtData, frame->nb_samples, (const uint8_t **) frame->data,
                                        frame->nb_samples);
                            outputFrame->data[0] = cvtData[0];
                            outputFrame->linesize[0] = cvtLinesize;
                        } else {
                            av_frame_ref(outputFrame, frame);
                        }

                        alCall(alBufferData, buffer, AL_FORMAT_STEREO16, outputFrame->data[0],
                               av_samples_get_buffer_size(&outputFrame->linesize[0], 2, frame->nb_samples, AV_SAMPLE_FMT_S16, 1),
                               frame->sample_rate);

                        alCall(alSourceQueueBuffers, d->m_ALSource, 1, &buffer);

                        av_frame_free(&frame);
                        if (d->m_pSwrContext) {
                            av_freep(&outputFrame->data[0]);
                        }
                        av_frame_free(&outputFrame);
                    } else {
                        msleep(4);
                    }
                }
                usleep(500);
            } else {
                usleep(500);
            }
        }

        alCall(alSourceStop, d->m_ALSource);

        alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);
    }

    void OpenALAudioOutput::syncToOutput(OpenGLRenderer *renderer) {
        Q_D(AVQt::OpenALAudioOutput);
        if (d->m_clock->isActive()) {
            d->m_clock->stop();
        }
        delete d->m_clock;
        d->m_clock = renderer->getClock();
    }

    void OpenALAudioOutput::clockIntervalChanged(int64_t interval) {
        Q_D(AVQt::OpenALAudioOutput);
        d->m_outputSliceDurationChanged.store(true);
    }
}