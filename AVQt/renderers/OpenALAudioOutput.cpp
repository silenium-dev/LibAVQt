#include "OpenALAudioOutput.h"
#include "renderers/private/OpenALAudioOutput_p.h"

#include "renderers/private/OpenALErrorHandler.h"

extern "C" {
#include <libswresample/swresample.h>
}

#include <QThread>
#include <output/IFrameSink.h>

namespace AVQt {
    OpenALAudioOutput::OpenALAudioOutput(QObject *parent) : QThread(parent), d_ptr(new OpenALAudioOutputPrivate(this)) {
    }

    [[maybe_unused]] OpenALAudioOutput::OpenALAudioOutput(OpenALAudioOutputPrivate &p) : d_ptr(&p) {
    }

    OpenALAudioOutput::OpenALAudioOutput(OpenALAudioOutput &&other) noexcept
        : d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    bool OpenALAudioOutput::isPaused() {
        Q_D(AVQt::OpenALAudioOutput);
        return false;
    }

    int OpenALAudioOutput::init(IAudioSource *source, int64_t duration, int sampleRate, AVSampleFormat sampleFormat, uint64_t channelLayout) {
        Q_D(AVQt::OpenALAudioOutput);

        if (source == nullptr || duration < 0 || sampleRate < 1 || sampleFormat == AV_SAMPLE_FMT_NONE || channelLayout < 0) {
            qWarning("[AVQt::OpenALAudioOutput] Invalid parameters passed to open()");
            return EINVAL;
        }

        IAudioSource *shouldBe = nullptr;
        if (!d->m_sourceLock.compare_exchange_strong(shouldBe, source)) {
            qWarning("[AVAt::OpenALAudioOutput] Output was locked on source in open(), please call close() from this source before reusing output");
            return EACCES;
        }

        d->m_pSwrContext = swr_alloc_set_opts(nullptr,
                                              OpenALAudioOutputPrivate::OUT_CHANNEL_LAYOUT, OpenALAudioOutputPrivate::OUT_SAMPLE_FORMAT, OpenALAudioOutputPrivate::OUT_SAMPLE_RATE,
                                              static_cast<int>(channelLayout), sampleFormat, sampleRate,
                                              0, nullptr);
        d->m_sampleFormat = sampleFormat;
        d->m_sampleRate = sampleRate;
        d->m_channelLayout = channelLayout;
        if (swr_init(d->m_pSwrContext) != 0) {
            qFatal("[AVQt::OpenALAudioOutput] Could not initialize SWR context");
        }

        {
            d->m_alcDevice = alcOpenDevice(nullptr);
            if (!d->m_alcDevice) {
                qFatal("[AVQt::OpenALAudioOutput] Could not open ALC device");
            }
            if (!alcCall(alcCreateContext, d->m_alcContext, d->m_alcDevice, d->m_alcDevice, nullptr) || !d->m_alcContext) {
                qFatal("[AVQt::OpenALAudioOutput] Could not create ALC context");
            }
            ALCboolean contextCurrent = ALC_FALSE;
            if (!alcCall(alcMakeContextCurrent, contextCurrent, d->m_alcDevice, d->m_alcContext) || contextCurrent != ALC_TRUE) {
                qFatal("[AVQt::OpenALAudioOutput] Could not make ALC context current");
            }
            d->m_alBuffers.resize(d->AL_BUFFER_COUNT);
            alCall(alGenBuffers, d->AL_BUFFER_COUNT, d->m_alBuffers.data());
            d->m_alBufferQueue.append(d->m_alBuffers.toList());

            alCall(alGenSources, 1, &d->m_alSource);

            alCall(alSourcef, d->m_alSource, AL_PITCH, 1.0f);
            alCall(alSourcef, d->m_alSource, AL_GAIN, 1.0f);
            alCall(alSource3f, d->m_alSource, AL_POSITION, 0.f, 0.f, 0.f);
            alCall(alSource3f, d->m_alSource, AL_VELOCITY, 0.f, 0.f, 0.f);
            alCall(alSourcei, d->m_alSource, AL_LOOPING, AL_FALSE);
            alCall(alSourcei, d->m_alSource, AL_BUFFER, 0);

            alcCall(alcMakeContextCurrent, contextCurrent, d->m_alcDevice, nullptr);
        }

        return 0;
    }

    int OpenALAudioOutput::deinit(IAudioSource *source) {
        Q_D(AVQt::OpenALAudioOutput);

        if (!d->m_sourceLock.compare_exchange_strong(source, nullptr)) {
            qWarning("[AVQt::OpenALAudioOutput] Calling close() from other source than locked on in open() is forbidden");
            return EACCES;
        }

        if (d->m_alcDevice) {
            stop(source);

            {
                ALCboolean contextCurrent = ALC_FALSE;
                alcCall(alcMakeContextCurrent, contextCurrent, d->m_alcDevice, d->m_alcContext);

                alCall(alSourcei, d->m_alSource, AL_SOURCE_STATE, AL_STOPPED);

                alCall(alDeleteSources, 1, &d->m_alSource);
                alCall(alDeleteBuffers, d->AL_BUFFER_COUNT, d->m_alBuffers.data());
                d->m_alBuffers.clear();
                d->m_alBuffers.squeeze();
                d->m_alBufferQueue.clear();

                alcCall(alcMakeContextCurrent, contextCurrent, d->m_alcDevice, nullptr);
                alcCall(alcDestroyContext, d->m_alcDevice, d->m_alcContext);

                ALCboolean closed = ALC_FALSE;
                alcCall(alcCloseDevice, closed, d->m_alcDevice, d->m_alcDevice);
            }
        }

        {
            QMutexLocker lock(&d->m_outputQueueMutex);
            for (auto &f : d->m_outputQueue) {
                av_frame_free(&f);
            }
            d->m_outputQueue.clear();
        }
        {
            QMutexLocker lock(&d->m_inputQueueMutex);
            for (auto &f : d->m_inputQueue) {
                av_frame_free(&f);
            }
            d->m_inputQueue.clear();
        }

        return 0;
    }

    int OpenALAudioOutput::start(IAudioSource *source) {
        Q_D(AVQt::OpenALAudioOutput);

        if (source != d->m_sourceLock) {
            qWarning("[AVQt::OpenALAudioOutput] Starting output from other source than locked on in open() is forbidden");
            return EACCES;
        }

        bool shouldBe = false;
        if (d->m_alcContext && d->m_running.compare_exchange_strong(shouldBe, true)) {// Check for alcContext to ensure open() was called
            QThread::start(QThread::TimeCriticalPriority);
            started();
        }


        return 0;
    }

    int OpenALAudioOutput::stop(IAudioSource *source) {
        Q_D(AVQt::OpenALAudioOutput);

        if (source != d->m_sourceLock) {
            qWarning("[AVQt::OpenALAudioOutput] Stopping output from other source than locked on in open() is forbidden");
            return EACCES;
        }

        bool shouldBe = true;
        if (d->m_running.compare_exchange_strong(shouldBe, false)) {
            // Clear output queue to prevent deadlock, because the processing thread is waiting
            // for space in the output queue, and we are waiting for the processing thread to finish
            {
                QMutexLocker lock(&d->m_outputQueueMutex);
                while (!d->m_outputQueue.isEmpty()) {
                    auto frame = d->m_outputQueue.dequeue();
                    av_frame_free(&frame);
                }
            }
            {
                QMutexLocker lock(&d->m_inputQueueMutex);
                while (!d->m_inputQueue.isEmpty()) {
                    auto frame = d->m_inputQueue.dequeue();
                    av_frame_free(&frame);
                }
            }
            wait();
        }

        stopped();

        return 0;
    }

    void OpenALAudioOutput::pause(IAudioSource *source, bool pause) {
        Q_D(AVQt::OpenALAudioOutput);

        ALCboolean contextCurrent = ALC_FALSE;
        if (!alcCall(alcMakeContextCurrent, contextCurrent, d->m_alcDevice, d->m_alcContext) || !contextCurrent) {
            qFatal("[AVQt::OpenALAudioOutput] Error making ALC context current");
        }
        qDebug("Setting paused state to: %d", pause);
        if (pause) {
            alCall(alSourcePause, d->m_alSource);
        } else {
            alCall(alSourcePlay, d->m_alSource);
        }
        alcCall(alcMakeContextCurrent, contextCurrent, d->m_alcDevice, nullptr);
    }

    void OpenALAudioOutput::onAudioFrame(IAudioSource *source, AVFrame *frame, uint32_t duration) {
        Q_D(AVQt::OpenALAudioOutput);
        if (d->m_sourceLock != source) {
            qWarning("[AVQt::OpenALAudioOutput] No audio samples from an other source than the one locked on in open() are allowed");
            return;
        }
        if (frame->sample_rate != d->m_sampleRate || frame->format != d->m_sampleFormat || frame->channel_layout != d->m_channelLayout) {
            qWarning("[AVQt::OpenALAudioOutput] Frame passed to onAudioFrame() has an invalid sample rate, sample format or channel layout");
            return;
        }
        AVFrame *queueFrame = av_frame_clone(frame);
        queueFrame->pts = frame->pts;

        qDebug("[AVQt::OpenALAudioOutput] Frame input queue size: %d", d->m_inputQueue.size());
        while (d->m_inputQueue.size() + d->m_outputQueue.size() > 500) {
            QThread::usleep(10);
        }
        {
            QMutexLocker lock(&d->m_inputQueueMutex);
            d->m_inputQueue.enqueue(queueFrame);
        }
        char strBuf[64];
        qDebug("[AVQt::OpenALAudioOutput] Got audio frame with format %s and pts %ld", av_get_sample_fmt_string(strBuf, 64, static_cast<AVSampleFormat>(frame->format)), frame->pts);
    }

    void OpenALAudioOutput::enqueueAudioForFrame(qint64 pts, qint64 duration) {
        Q_D(AVQt::OpenALAudioOutput);
        if (d->m_alcDevice) {
            qDebug("[AVQt::OpenALAudioOutput] frame with PTS %lld and duration %lld is being processed", pts, duration);

            ALCboolean contextCurrent = ALC_FALSE;
            if (!alcCall(alcMakeContextCurrent, contextCurrent, d->m_alcDevice, d->m_alcContext) || !contextCurrent) {
                qFatal("[AVQt::OpenALAudioOutput] Error making ALC context current");
            }

            while (alGetError() != AL_NO_ERROR) {
            }

            {
                ALint buffersProcessed = 0;
                alGetSourcei(d->m_alSource, AL_BUFFERS_PROCESSED, &buffersProcessed);
                qDebug("Buffers processed: %d", buffersProcessed);
                ALint buffersQueued = 0;
                alGetSourcei(d->m_alSource, AL_BUFFERS_QUEUED, &buffersQueued);
                qDebug("Buffers queued: %d", buffersQueued);

                ALuint buffer = 0;
                while (buffersProcessed--) {
                    alSourceUnqueueBuffers(d->m_alSource, 1, &buffer);
                    d->m_alBufferQueue.enqueue(buffer);
                }
            }

            QVector<AVFrame *> audioQueue;
            if (!d->m_outputQueue.isEmpty()) {
                AVFrame *frame = frame = d->m_outputQueue.dequeue();
                QMutexLocker lock(&d->m_outputQueueMutex);
                while (!d->m_outputQueue.isEmpty() && frame->pts < pts) {
                    qDebug("[AVQt::OpenALAudioOutput] Skipping frame with PTS %ld", frame->pts);
                    av_frame_free(&frame);
                    frame = d->m_outputQueue.dequeue();
                }
                int64_t queuedDuration = frame->nb_samples / frame->sample_rate * 1000000;
                audioQueue.append(frame);
                size_t frames = 0;
                while (!d->m_outputQueue.isEmpty() && queuedDuration < duration) {
                    frame = d->m_outputQueue.dequeue();
                    queuedDuration += frame->nb_samples / frame->sample_rate * 1000000;
                    audioQueue.append(frame);
                    ++frames;
                }
                qDebug("[AVQt::OpenALAudioOutput] Enqueuing %zu frames", frames);
            }
            std::vector<uint8_t> audioData;
            QVector<ALuint> buffers;
            bool hasData = false;
            for (auto &f : audioQueue) {
                hasData = true;
                audioData.resize(av_samples_get_buffer_size(nullptr, f->channels, f->nb_samples, static_cast<AVSampleFormat>(f->format), 1));
                memcpy(audioData.data(), f->data[0], audioData.size());
                if (d->m_alBufferQueue.isEmpty()) {
                    qDebug("[AVQt::OpenALAudioOutput] Allocating additional buffers");
                    int oldSize = d->m_alBuffers.size();
                    d->m_alBuffers.resize(d->m_alBuffers.size() + OpenALAudioOutputPrivate::AL_BUFFER_ALLOC_STEPS);
                    alCall(alGenBuffers, OpenALAudioOutputPrivate::AL_BUFFER_ALLOC_STEPS, d->m_alBuffers.data() + oldSize);
                    d->m_alBufferQueue.append(d->m_alBuffers.mid(oldSize).toList());
                    d->AL_BUFFER_COUNT += OpenALAudioOutputPrivate::AL_BUFFER_ALLOC_STEPS;
                }
                auto buffer = d->m_alBufferQueue.dequeue();
                alBufferData(buffer, AL_FORMAT_STEREO_FLOAT32, audioData.data(), static_cast<ALsizei>(audioData.size()), f->sample_rate);
                buffers.append(buffer);
                audioData.clear();
            }
            alSourceQueueBuffers(d->m_alSource, buffers.size(), buffers.data());

            if (hasData) {
                bool shouldBe = true;
                if (d->m_firstFrame.compare_exchange_strong(shouldBe, false)) {
                    alCall(alSourcePlay, d->m_alSource);
                    qDebug("[AVQt::OpenALAudioOutput] Starting source");
                }
            }

            alcCall(alcMakeContextCurrent, contextCurrent, d->m_alcDevice, nullptr);
        }
    }

    void OpenALAudioOutput::run() {
        Q_D(AVQt::OpenALAudioOutput);
        while (d->m_running) {
            if (d->m_inputQueue.isEmpty()) {
                msleep(1);
            } else {
                AVFrame *queueFrame, *frame;
                {
                    QMutexLocker lock(&d->m_inputQueueMutex);
                    frame = d->m_inputQueue.dequeue();
                }
                if (frame->sample_rate == OpenALAudioOutputPrivate::OUT_SAMPLE_RATE && frame->format == AV_SAMPLE_FMT_FLT && frame->channel_layout == AV_CH_LAYOUT_STEREO) {
                    queueFrame = av_frame_clone(frame);
                } else {
                    queueFrame = av_frame_alloc();
                    queueFrame->format = OpenALAudioOutputPrivate::OUT_SAMPLE_FORMAT;
                    queueFrame->channel_layout = OpenALAudioOutputPrivate::OUT_CHANNEL_LAYOUT;
                    queueFrame->sample_rate = OpenALAudioOutputPrivate::OUT_SAMPLE_RATE;
                    queueFrame->pts = frame->pts;
                    int ret;
                    if ((ret = swr_convert_frame(d->m_pSwrContext, queueFrame, frame)) != 0) {
                        char strBuf[64];
                        qFatal("[AVQt::OpenALAudioOutput] Error converting audio frame: %s", av_make_error_string(strBuf, 64, ret));
                    }
                }
                qDebug("[AVQt::OpenALAudioOutput] Frame output queue size: %d", d->m_outputQueue.size());
                while (d->m_outputQueue.size() > 200) {
                    msleep(1);
                }
                {
                    QMutexLocker lock(&d->m_outputQueueMutex);
                    d->m_outputQueue.enqueue(queueFrame);
                }
            }
        }
    }
    void OpenALAudioOutput::syncToOutput(AVQt::IFrameSink *output) {
        Q_D(AVQt::OpenALAudioOutput);
        if (d->m_alcDevice) {// Only connect after initialization
            connect(dynamic_cast<QObject *>(output), SIGNAL(frameProcessingStarted(qint64, qint64)), this, SLOT(enqueueAudioForFrame(qint64, qint64)), Qt::QueuedConnection);
        }
    }
}// namespace AVQt
