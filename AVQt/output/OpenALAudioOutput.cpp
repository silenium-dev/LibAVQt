//
// Created by silas on 4/28/21.
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
    }

    OpenALAudioOutput::OpenALAudioOutput(OpenALAudioOutput &&other) noexcept: d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
    }

    int OpenALAudioOutput::init(IAudioSource *source, int64_t duration, int sampleRate) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenALAudioOutput);

        qDebug("Initializing OpenALAudioOutput");

        if (d->m_running.load() || !d->m_ALBuffers.empty()) {
            return -1;
        }

//        if (!d->m_clock) {
//            d->m_clock = new RenderClock;
//            d->m_clock.load()->setInterval(10);
//        }

        d->m_duration = duration;
        d->m_sampleRate = sampleRate;

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

        d->m_ALBufferCount = static_cast<int>(std::ceil((1000 * 1.0 / 60) * (static_cast<double>(d->m_sampleRate) / 1536.0)));
        qDebug("Using %d OpenAL buffers", d->m_ALBufferCount);

        d->m_ALBuffers.resize(d->m_ALBufferCount);

        alCall(alGenBuffers, d->m_ALBufferCount, d->m_ALBuffers.data());

        alCall(alGenSources, 1, &d->m_ALSource);
        alCall(alSourcef, d->m_ALSource, AL_PITCH, 1.0f);
        alCall(alSourcef, d->m_ALSource, AL_GAIN, 1.0f);
        alCall(alSource3f, d->m_ALSource, AL_POSITION, 0.f, 0.f, 0.f);
        alCall(alSource3f, d->m_ALSource, AL_VELOCITY, 0.f, 0.f, 0.f);
        alCall(alSourcei, d->m_ALSource, AL_LOOPING, AL_FALSE);
        alCall(alSourcei, d->m_ALSource, AL_BUFFER, 0);

//        constexpr size_t SAMPLE_COUNT = 50;
//        uint8_t **data = nullptr;
//        int linesize = 0;
//        av_samples_alloc_array_and_samples(&data, &linesize, 2, SAMPLE_COUNT, AV_SAMPLE_FMT_S16, 1);
//        av_samples_set_silence(data, 0, SAMPLE_COUNT, 2, AV_SAMPLE_FMT_S16);
//
//        for (size_t i = 0; i < 10; ++i) {
//            alCall(alBufferData, d->m_ALBuffers[i], AL_FORMAT_STEREO16, &data[0][0],
//                   av_samples_get_buffer_size(&linesize, 2, SAMPLE_COUNT, AV_SAMPLE_FMT_S16, 1), 48000);
//        }
//
//        av_freep(&data[0]);

//        linesize = 0;
//        av_samples_alloc_array_and_samples(&data, &linesize, 2, 1, AV_SAMPLE_FMT_S16, 1);
//        av_samples_set_silence(data, 0, 1, 2, AV_SAMPLE_FMT_S16);
//
//        for (size_t i = 11; i < d->m_bufferCount; ++i) {
//            alCall(alBufferData, d->m_ALBuffers[i], AL_FORMAT_STEREO16, &data[0][0],
//                   av_samples_get_buffer_size(&linesize, 2, 1, AV_SAMPLE_FMT_S16, 1), 48000);
//        }
//
//        av_freep(&data[0]);
//
//        alCall(alSourceQueueBuffers, d->m_ALSource, 10, &d->m_ALBuffers[0]);

        alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);

        return 0;
    }

    int OpenALAudioOutput::deinit(IAudioSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenALAudioOutput);

        stop(source);

        ALCboolean contextCurrent = ALC_FALSE;
        alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, d->m_ALCContext);

        alCall(alSourcei, d->m_ALSource, AL_SOURCE_STATE, AL_STOPPED);

        alCall(alDeleteSources, 1, &d->m_ALSource);
        alCall(alDeleteBuffers, d->m_ALBufferCount, d->m_ALBuffers.data());
        d->m_ALBuffers.clear();
        d->m_ALBuffers.squeeze();

        alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);
        alcCall(alcDestroyContext, d->m_ALCDevice, d->m_ALCContext);

        ALCboolean closed = ALC_FALSE;
        alcCall(alcCloseDevice, closed, d->m_ALCDevice, d->m_ALCDevice);

        return 0;
    }

    int OpenALAudioOutput::start(IAudioSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenALAudioOutput);

//        if (d->m_clock) {
//            d->m_clock.load()->start();
//        }

        bool shouldBeCurrent = false;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, true)) {
            d->m_paused.store(false);
            d->m_ALBufferQueue.clear();
            d->m_ALBufferQueue.append(d->m_ALBuffers.toList());
            QThread::start(QThread::TimeCriticalPriority);
            return 0;
        }
        return -1;
    }

    int OpenALAudioOutput::stop(IAudioSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenALAudioOutput);

        bool shouldBeCurrent = true;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, false)) {
            if (d->m_clock.load()->isActive()) {
                qDebug("Clock active, stopping");
//                d->m_clock.load()->stop();
                if (d->m_clock.load()->thread() != QThread::currentThread()) {
                    QMetaObject::invokeMethod(d->m_clock, "stop", Qt::BlockingQueuedConnection);
                } else {
                    d->m_clock.load()->stop();
                }
            }

            quit();

            {
                QMutexLocker lock(&d->m_inputQueueMutex);
                for (auto &frame: d->m_inputQueue) {
                    av_frame_unref(frame.first);
                    av_frame_free(&frame.first);
                }
                d->m_inputQueue.clear();
            }
            {
                QMutexLocker lock(&d->m_outputQueueMutex);
                for (auto &frame: d->m_outputQueue) {
                    av_frame_unref(frame.first);
                    av_frame_free(&frame.first);
                }
                d->m_inputQueue.clear();
            }
            {
                ALboolean contextCurrent = AL_FALSE;
                alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, d->m_ALCContext);
                alCall(alSourceStop, d->m_ALSource);
                QMutexLocker lock(&d->m_ALBufferQueueMutex);
                ALuint buffer;
                while (alCall(alSourceUnqueueBuffers, d->m_ALSource, 1, &buffer)) {
                    d->m_ALBufferQueue.enqueue(buffer);
                }
                alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);
            }

//            pause(nullptr, true);
            wait();
            qDebug("OpenALAudioOutput at %#lx stopped", reinterpret_cast<int64_t>(this));

            stopped();

            return 0;
        }
        return -1;
    }

    void OpenALAudioOutput::pause(IAudioSource *source, bool pause) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenALAudioOutput);

        bool shouldBeCurrent = !pause;
        if (d->m_paused.compare_exchange_strong(shouldBeCurrent, pause)) {
            ALCboolean contextCurrent = ALC_FALSE;
            if (!alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, d->m_ALCContext) || contextCurrent != ALC_TRUE) {
                qFatal("Could not make ALC context current");
            }

            if (pause) {
                alCall(alSourcePause, d->m_ALSource);
//                clockTriggered();
            } else {
                alCall(alSourceRewind, d->m_ALSource);
                alCall(alSourcePlay, d->m_ALSource);
            }

            alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);

            paused(pause);
        }
    }

    bool OpenALAudioOutput::isPaused() {
        Q_D(AVQt::OpenALAudioOutput);
        return d->m_paused.load();
    }

    void OpenALAudioOutput::onAudioFrame(IAudioSource *source, AVFrame *frame, uint32_t duration) {
        Q_UNUSED(source)
        Q_D(AVQt::OpenALAudioOutput);

        QPair<AVFrame *, int64_t> newFrame;

        newFrame.first = av_frame_alloc();

        newFrame.second = duration;

        {
            QMutexLocker lock(&d->m_swrContextMutex);
            if (!d->m_pSwrContext && frame->format != AV_SAMPLE_FMT_S16) {
                d->m_pSwrContext = swr_alloc_set_opts(nullptr, static_cast<int64_t>(frame->channel_layout), AV_SAMPLE_FMT_S16,
                                                      frame->sample_rate,
                                                      static_cast<int64_t>(frame->channel_layout),
                                                      static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                                                      0, nullptr);
                swr_init(d->m_pSwrContext);
            }
            if (d->m_pSwrContext) {
//                qDebug("Frame not in S16, converting...");
                newFrame.first->format = AV_SAMPLE_FMT_S16;
                newFrame.first->sample_rate = frame->sample_rate;
                newFrame.first->channels = 2;
                newFrame.first->channel_layout = AV_CH_LAYOUT_STEREO;
                newFrame.first->nb_samples = static_cast<int>(std::ceil(
                        (frame->nb_samples * 1.0 / frame->channels) * newFrame.first->channels));
                newFrame.first->pts = frame->pts;
                av_frame_get_buffer(newFrame.first, 1);
                int ret = swr_convert_frame(d->m_pSwrContext, newFrame.first, frame);
                if (newFrame.first->data[1] || newFrame.first->linesize[0] == 0) {
                    qFatal("Error in frame conversion");
                }
                if (ret < 0) {
                    constexpr size_t strBufSize = 64;
                    char strBuf[strBufSize];
                    qFatal("%d: Error convertion audio frame: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }
            } else {
                lock.unlock();
//                qDebug("Frame already in S16");
                av_frame_ref(newFrame.first, frame);
            }
        }

        while (d->m_inputQueue.size() >= 1000) {
            QThread::msleep(4);
        }

        QMutexLocker lock(&d->m_inputQueueMutex);
        d->m_inputQueue.enqueue(newFrame);
    }

    void OpenALAudioOutput::run() {
        Q_D(AVQt::OpenALAudioOutput);

        if (!d->m_clock) {
            d->m_clock = new RenderClock;
            d->m_clock.load()->setInterval(16);
            d->m_clockInterval = d->m_clock.load()->getInterval();
            connect(d->m_clock, &RenderClock::timeout, this, &OpenALAudioOutput::clockTriggered);
            d->m_clock.load()->start();
        }

        exec();
    }

    [[maybe_unused]] void OpenALAudioOutput::syncToOutput(OpenGLRenderer *renderer) {
        Q_D(AVQt::OpenALAudioOutput);
        if (!d->m_running.load()) {
            if (d->m_clock.load()) {
                d->m_clock.load()->stop();
                delete d->m_clock.load();
            }

            d->m_clock.store(renderer->getClock());
            clockIntervalChanged(d->m_clock.load()->getInterval());
            int newBufferCount = static_cast<int>(std::ceil(
                    (1000 * 1.0 / static_cast<double>(d->m_clockInterval) * static_cast<double>(d->m_sampleRate) / 1536)));
            if (d->m_ALBuffers.size() != newBufferCount) {
                ALCboolean contextCurrent = ALC_FALSE;
                alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, d->m_ALCContext);
                if (d->m_ALBuffers.size() > newBufferCount) {
                    int overhead = static_cast<int>(d->m_ALBuffers.size() - newBufferCount);
                    alCall(alDeleteBuffers, overhead, d->m_ALBuffers.data() + d->m_ALBuffers.size() - overhead);
                    d->m_ALBuffers.resize(newBufferCount);
                } else if (d->m_ALBuffers.size() < newBufferCount) {
                    int extraBuffers = static_cast<int>(newBufferCount - d->m_ALBuffers.size());
                    d->m_ALBuffers.resize(newBufferCount);
                    alCall(alGenBuffers, extraBuffers, d->m_ALBuffers.data() + d->m_ALBuffers.size() - extraBuffers);
                }
                alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);
            }
            connect(d->m_clock.load(), &RenderClock::intervalChanged, this, &OpenALAudioOutput::clockIntervalChanged);
            connect(d->m_clock.load(), &RenderClock::timeout, this, &OpenALAudioOutput::clockTriggered);
            connect(renderer, &OpenGLRenderer::paused, [this](bool paused) {
                QMetaObject::invokeMethod(this, "pause", Q_ARG(IAudioSource *, nullptr), Q_ARG(bool, paused));
            });
        }
    }

    void OpenALAudioOutput::clockIntervalChanged(int64_t interval) {
        Q_D(AVQt::OpenALAudioOutput);
        d->m_clockInterval = interval;
        d->m_outputSliceDurationChanged.store(true);
    }

    void OpenALAudioOutput::clockTriggered(qint64 timestamp) {
        Q_D(AVQt::OpenALAudioOutput);

        // If no buffers generated, the audio output was not initialized
        if (d->m_ALBuffers.isEmpty()) {
            return;
        }

        if (timestamp - d->m_lastUpdate.load() > d->m_clockInterval * 5000) {
            qDebug("Was paused since last update for %lld ms", (timestamp - d->m_lastUpdate.load()) / 1000);
            d->m_wasPaused.store(true);
        }
        d->m_lastUpdate.store(timestamp);
//            qDebug() << "Playing frame";
        ALCboolean contextCurrent = ALC_FALSE;
        if (!alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, d->m_ALCContext) || contextCurrent != ALC_TRUE) {
            qFatal("Could not make ALC context current");
        }

//        qDebug("Buffers processed: %d", buffersProcessed);
        bool shouldBe = true;
        if (d->m_wasPaused.compare_exchange_strong(shouldBe, false)) {
            alCall(alSourceStop, d->m_ALSource);
            QVector<ALuint> buffers;
            ALuint buffer;
            while (alCall(alSourceUnqueueBuffers, d->m_ALSource, 1, &buffer)) {
//                qDebug("Unqueued buffer %d", buffer);
                buffers.append(buffer);
                QMutexLocker lock(&d->m_ALBufferSampleMapMutex);
                d->m_queuedSamples -= d->m_ALBufferSampleMap[buffer];
            }
            if (!d->m_ALBufferQueue.isEmpty()) {
                QMutexLocker lock3(&d->m_ALBufferQueueMutex);
                alCall(alSourceQueueBuffers, d->m_ALSource, static_cast<int>(qMin(d->m_ALBufferQueue.size(), 4)),
                       d->m_ALBufferQueue.toVector().data());
                for (const auto &buf: d->m_ALBufferQueue.mid(d->m_ALBufferQueue.size() - static_cast<int>(qMin(d->m_ALBufferQueue.size(), 4)))) {
                    d->m_queuedSamples += d->m_ALBufferSampleMap[buf];
                }
                if (d->m_ALBufferQueue.size() < 4) {
                    alCall(alSourceQueueBuffers, d->m_ALSource, static_cast<int>(qMin(buffers.size(), 4 - d->m_ALBufferQueue.size())),
                           buffers.data());
                    for (qint64 i = static_cast<int>(buffers.size() - qMin(buffers.size(), 4 - d->m_ALBufferQueue.size()));
                         i < buffers.size(); ++i) {
                        d->m_queuedSamples += d->m_ALBufferSampleMap[buffers[i]];
                    }
                    buffers.remove(buffers.size() - qMin(buffers.size(), 4 - d->m_ALBufferQueue.size()),
                                   qMin(buffers.size(), 4 - d->m_ALBufferQueue.size()));
                    d->m_ALBufferQueue.append(buffers.toList());
                }
                d->m_ALBufferQueue.clear();
            } else {
                QMutexLocker lock3(&d->m_ALBufferQueueMutex);
                alCall(alSourceQueueBuffers, d->m_ALSource, static_cast<int>(qMin(buffers.size(), 4)), buffers.data());
                buffers.remove(buffers.size() - qMin(buffers.size(), 4), qMin(buffers.size(), 4));
                d->m_ALBufferQueue.append(buffers.toList());
            }
        } else {
            ALint buffersProcessed = 0;
            alCall(alGetSourcei, d->m_ALSource, AL_BUFFERS_PROCESSED, &buffersProcessed);
//                alCall(alSourceStop, d->m_ALSource);
            if (buffersProcessed > 0) {
                d->m_playingBuffers -= buffersProcessed;
                while (buffersProcessed--) {
                    ALuint buffer;
                    alCall(alSourceUnqueueBuffers, d->m_ALSource, 1, &buffer);
//                qDebug("Unqueued buffer %d", buffer);
                    QMutexLocker lock1(&d->m_ALBufferQueueMutex);
                    d->m_ALBufferQueue.enqueue(buffer);
                    lock1.unlock();
                    QMutexLocker lock(&d->m_ALBufferSampleMapMutex);
                    d->m_queuedSamples -= d->m_ALBufferSampleMap[buffer];
                }
//            qDebug("Unqueued buffers from OpenAL-Source");
//            qDebug() << d->m_ALBufferQueue;
            }
//        if (d->m_paused.load()) {
//            QQueue<QPair<ALuint, int64_t>> buffers;
//            QMutexLocker lock3(&d->m_ALBufferQueueMutex);
//            QMutexLocker lock4(&d->m_ALBufferSampleMapMutex);
//            ALuint buffer;
//            while (buffers.size() < 6) {
//                buffers.enqueue({d->m_ALBufferQueue.back(), d->m_ALBufferSampleMap[d->m_ALBufferQueue.back()]});
//                d->m_ALBufferSampleMap.remove(d->m_ALBufferQueue.back());
//                d->m_ALBufferQueue.pop_back();
//            }
//            while (d->m_playingBuffers.load() > 0) {
//                alCall(alSourceUnqueueBuffers, d->m_ALSource, 1, &buffer);
//                buffers.enqueue({buffer, d->m_ALBufferSampleMap[buffer]});
//                d->m_ALBufferSampleMap.remove(buffer);
//                --d->m_playingBuffers;
//            }
//            while (!buffers.empty()) {
//                auto buf = buffers.dequeue();
//                alCall(alSourceQueueBuffers, d->m_ALSource, 1, &buf.first);
//                d->m_ALBufferSampleMap[buf.first] = buf.second;
//            }
//        }
            if (d->m_inputQueue.isEmpty() || d->m_paused.load()) {
                return;
            }
            qDebug("Size of output queue: %d", d->m_inputQueue.size());
            QMutexLocker lock(&d->m_ALBufferQueueMutex);
            if (!d->m_ALBufferQueue.isEmpty()) {
                lock.unlock();
                int64_t duration = 0;
//                QTime time = QTime::currentTime();


                QPair<AVFrame *, int64_t> frame;
                {
                    QMutexLocker lock2(&d->m_inputQueueMutex);
                    if (d->m_inputQueue.first().first->pts >= timestamp + d->m_clockInterval * 1000) {
                        alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);
                        return;
                    }

                    while (!d->m_inputQueue.isEmpty()) {
                        if (d->m_inputQueue.first().first->pts / 1000 >= timestamp / 1000) {
                            break;
                        }
                        frame = d->m_inputQueue.dequeue();
                        qDebug("Discarding audio frame at PTS: %ld < PTS: %lld", frame.first->pts, timestamp);
                        av_frame_unref(frame.first);
                        av_frame_free(&frame.first);
                    }
                }
                while (!d->m_inputQueue.isEmpty() &&
                       !d->m_ALBufferQueue.isEmpty()) {
                    if (static_cast<double>(d->m_queuedSamples) * 1.0 / d->m_inputQueue.front().first->sample_rate <
                        static_cast<double>(d->m_clockInterval) * 1.5) {
                        lock.relock();
                        QMutexLocker lock1(&d->m_inputQueueMutex);
                        QMutexLocker lock2(&d->m_ALBufferSampleMapMutex);
//                    if (d->m_inputQueue.size() >= 2) {
//                        frame1 = d->m_inputQueue.dequeue();
//                        frame2 = d->m_inputQueue.dequeue();
//                        frame.first = av_frame_alloc();
//                        frame.first->sample_rate = frame1.first->sample_rate;
//                        frame.first->nb_samples = frame1.first->nb_samples + frame2.first->nb_samples;
//                        frame.first->channels = frame1.first->channels;
//                        frame.first->format = frame1.first->format;
//                        av_frame_get_buffer(frame.first, 1);
//                        memcpy(frame.first->buf[0]->data, frame1.first->buf[0]->data, frame1.first->buf[0]->size);
//                        memcpy(frame.first->buf[0]->data + frame1.first->buf[0]->size, frame2.first->buf[0]->data, frame2.first->buf[0]->size);
//                        frame.second = frame1.second + frame2.second;
//                        av_frame_free(&frame1.first);
//                        av_frame_free(&frame2.first);
//                    } else {
                        frame = d->m_inputQueue.dequeue();
//                    }
                        duration += static_cast<int64_t>(std::round(frame.first->nb_samples * 1000.0 / frame.first->sample_rate));
                        lock1.unlock();
                        auto buf = d->m_ALBufferQueue.dequeue();
                        lock.unlock();
//                    qDebug("Buffering data to buffer: %d", buf);
                        alCall(alBufferData,
                               buf,
                               AL_FORMAT_STEREO16,
                               &frame.first->buf[0]->data[0],
                               frame.first->buf[0]->size,
                               frame.first->sample_rate);
                        d->m_ALBufferSampleMap[buf] = frame.first->nb_samples;
                        d->m_queuedSamples += frame.first->nb_samples;
                        av_frame_free(&frame.first);
                        alCall(alSourceQueueBuffers, d->m_ALSource, 1, &buf);
                        ++d->m_playingBuffers;
                        ++d->m_audioFrame;
                    } else {
                        break;
                    }
                }
            }
        }

        ALint state = 0;
        alCall(alGetSourcei, d->m_ALSource, AL_SOURCE_STATE, &state);
//        qDebug() << QString("Playing ?: ") + QString((state == AL_PLAYING) ? "true" : "false");
        if (state != AL_PLAYING) {
            alCall(alSourcePlay, d->m_ALSource);
        }

        alcCall(alcMakeContextCurrent, contextCurrent, d->m_ALCDevice, nullptr);
    }
}
