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

#include "AudioDecoder.hpp"
#include "AVQt/decoder/private/AudioDecoder_p.hpp"

#include "AVQt/input/IPacketSource.hpp"
#include "output/IAudioSink.hpp"

#include <QtCore>

#include <AL/alext.h>

namespace AVQt {
    AudioDecoder::AudioDecoder(QObject *parent) : QThread(parent), d_ptr(new AudioDecoderPrivate(this)) {
    }

    [[maybe_unused]] AudioDecoder::AudioDecoder(AVQt::AudioDecoderPrivate &p) : d_ptr(&p) {
    }

    AudioDecoder::AudioDecoder(AudioDecoder &&other) noexcept : d_ptr(other.d_ptr) {
        d_ptr->q_ptr = this;
        other.d_ptr = nullptr;
    }

    AudioDecoder::~AudioDecoder() {
        delete d_ptr;
        d_ptr = nullptr;
    }

    int AudioDecoder::init() {
        Q_D(AVQt::AudioDecoder);

        //        {
        //            QMutexLocker lock(&d->m_cbListMutex);
        //            for (const auto &cb: d->m_cbList) {
        //                cb->open(this, d->m_duration, 0, AV_SAMPLE_FMT_U8, 0);
        //            }
        //        }

        return 0;
    }

    int AudioDecoder::deinit() {
        Q_D(AVQt::AudioDecoder);

        if (d->m_pCodecParams) {
            avcodec_parameters_free(&d->m_pCodecParams);
            d->m_pCodecParams = nullptr;
        }
        if (d->m_pCodecCtx) {
            avcodec_close(d->m_pCodecCtx);
            avcodec_free_context(&d->m_pCodecCtx);
            d->m_pCodecCtx = nullptr;
            d->m_pCodec = nullptr;
        }

        return 0;
    }

    int AudioDecoder::start() {
        Q_D(AVQt::AudioDecoder);

        bool notRunning = false;
        if (d->m_running.compare_exchange_strong(notRunning, true)) {
            d->m_paused.store(false);
            paused(false);

            //            {
            //                QMutexLocker lock(&d->m_cbListMutex);
            //                for (const auto &cb: d->m_cbList) {
            //                    cb->start(this);
            //                }
            //            }

            QThread::start();
            started();
            return 0;
        }
        return -1;
    }

    int AudioDecoder::stop() {
        Q_D(AVQt::AudioDecoder);

        bool shouldBeCurrent = true;
        if (d->m_running.compare_exchange_strong(shouldBeCurrent, false)) {
            //            {
            //                QMutexLocker lock(&d->m_cbListMutex);
            for (const auto &cb : d->m_cbList) {
                cb->stop(this);
            }
            //            }
            {
                QMutexLocker lock(&d->m_inputQueueMutex);
                for (auto &packet : d->m_inputQueue) {
                    av_packet_unref(packet);
                    av_packet_free(&packet);
                }
                d->m_inputQueue.clear();
            }

            wait();

            stopped();

            return 0;
        }

        return -1;
    }

    void AudioDecoder::pause(bool pause) {
        Q_D(AVQt::AudioDecoder);

        bool shouldBeCurrent = !pause;

        if (d->m_paused.compare_exchange_strong(shouldBeCurrent, pause)) {
            paused(pause);
            QMutexLocker lock(&d->m_cbListMutex);
            for (const auto &cb : d->m_cbList) {
                cb->pause(this, pause);
            }
        }
    }

    bool AudioDecoder::isPaused() {
        Q_D(AVQt::AudioDecoder);
        return d->m_paused.load();
    }

    qint64 AudioDecoder::registerCallback(IAudioSink *callback) {
        Q_D(AVQt::AudioDecoder);

        QMutexLocker lock(&d->m_cbListMutex);
        if (!d->m_cbList.contains(callback)) {
            d->m_cbList.append(callback);

            if (d->m_pCodecCtx) {
                callback->init(this, d->m_duration, d->m_pCodecParams->sample_rate, static_cast<AVSampleFormat>(d->m_pCodecParams->format), d->m_pCodecCtx->channel_layout);
            }

            if (d->m_running) {
                callback->start(this);
            }
            return d->m_cbList.indexOf(callback);
        }

        return -1;
    }

    qint64 AudioDecoder::unregisterCallback(IAudioSink *callback) {
        Q_D(AVQt::AudioDecoder);

        QMutexLocker lock(&d->m_cbListMutex);
        if (!d->m_cbList.contains(callback)) {
            d->m_cbList.removeAll(callback);
            return 0;
        }
        return -1;
    }

    void AudioDecoder::init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                            AVCodecParameters *aParams, AVCodecParameters *sParams) {
        Q_D(AVQt::AudioDecoder);
        Q_UNUSED(source)
        Q_UNUSED(framerate)
        Q_UNUSED(vParams)
        Q_UNUSED(sParams)

        d->m_timebase = timebase;
        d->m_duration = duration;

        d->m_pCodecParams = avcodec_parameters_alloc();
        avcodec_parameters_copy(d->m_pCodecParams, aParams);

        {
            QMutexLocker lock(&d->m_cbListMutex);
            for (const auto &cb : d->m_cbList) {
                cb->init(this, duration, aParams->sample_rate, static_cast<AVSampleFormat>(aParams->format), aParams->channel_layout);
            }
        }

        init();
    }

    void AudioDecoder::deinit(IPacketSource *source) {
        Q_UNUSED(source)

        deinit();
    }

    void AudioDecoder::start(IPacketSource *source) {
        Q_UNUSED(source)

        start();
    }

    void AudioDecoder::stop(IPacketSource *source) {
        Q_UNUSED(source)

        stop();
    }

    void AudioDecoder::onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) {
        Q_D(AVQt::AudioDecoder);
        Q_UNUSED(source)

        if (packetType == IPacketSource::CB_AUDIO) {
            AVPacket *queuePacket = av_packet_clone(packet);
            while (d->m_inputQueue.size() >= 100) {
                QThread::usleep(10);
            }
            QMutexLocker lock(&d->m_inputQueueMutex);
            d->m_inputQueue.append(queuePacket);
        }
    }

    void AudioDecoder::run() {
        Q_D(AVQt::AudioDecoder);

        while (d->m_running.load()) {
            if (!d->m_paused.load() && !d->m_inputQueue.isEmpty()) {
                QTime time1 = QTime::currentTime();
                qDebug("Audio packet queue size: %d", d->m_inputQueue.size());

                int ret;
                constexpr size_t strBufSize = 64;
                char strBuf[strBufSize];

                // If m_pCodecParams is nullptr, it is not initialized by packet source, if video codec context is nullptr, this is the first packet
                if (d->m_pCodecParams && !d->m_pCodecCtx) {
                    d->m_pCodec = avcodec_find_decoder(d->m_pCodecParams->codec_id);
                    if (!d->m_pCodec) {
                        qFatal("No audio decoder found");
                    }

                    d->m_pCodecCtx = avcodec_alloc_context3(d->m_pCodec);
                    if (!d->m_pCodecCtx) {
                        qFatal("Could not allocate audio decoder context, probably out of memory");
                    }

                    avcodec_parameters_to_context(d->m_pCodecCtx, d->m_pCodecParams);
                    ret = avcodec_open2(d->m_pCodecCtx, d->m_pCodec, nullptr);
                    if (ret != 0) {
                        qFatal("%d: Could not open audio decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    for (const auto &cb : d->m_cbList) {
                        cb->start(this);
                    }
                }

                {
                    QMutexLocker lock(&d->m_inputQueueMutex);
                    while (!d->m_inputQueue.isEmpty()) {
                        AVPacket *packet = d->m_inputQueue.dequeue();
                        lock.unlock();

                        qDebug("Audio packet queue size: %d", d->m_inputQueue.size());

                        ret = avcodec_send_packet(d->m_pCodecCtx, packet);
                        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                            lock.relock();
                            d->m_inputQueue.prepend(packet);
                            break;
                        } else if (ret < 0) {
                            qFatal("%d: Error sending packet to audio decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                        }
                        av_packet_unref(packet);
                        av_packet_free(&packet);
                        lock.relock();
                    }
                }

                AVFrame *frame = av_frame_alloc();
                while (true) {
                    ret = avcodec_receive_frame(d->m_pCodecCtx, frame);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret < 0) {
                        qFatal("%d: Error receiving frame from audio decoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    QMutexLocker lock2(&d->m_cbListMutex);
                    for (const auto &cb : d->m_cbList) {
                        AVFrame *cbFrame = av_frame_clone(frame);
                        cbFrame->pts = av_rescale_q(frame->pts, d->m_timebase,
                                                    av_make_q(1, 1000000));// Rescale pts to microseconds for easier processing
                        qDebug("Calling audio frame callback for PTS: %lld, Timebase: %d/%d", static_cast<long long>(cbFrame->pts),
                               d->m_timebase.num,
                               d->m_timebase.den);
                        QTime time = QTime::currentTime();
                        cb->onAudioFrame(this, cbFrame,
                                         static_cast<uint32_t>(frame->nb_samples * 1.0 / frame->sample_rate / frame->channels * 1000.0));
                        qDebug() << "Audio CB time:" << time.msecsTo(QTime::currentTime());
                        av_frame_unref(cbFrame);
                    }
                    int64_t sleepDuration = (frame->nb_samples / frame->sample_rate * 1000) - time1.msecsTo(QTime::currentTime());
                    msleep(sleepDuration <= 0 ? 0 : static_cast<unsigned long>(sleepDuration));
                    time1 = QTime::currentTime();
                    av_frame_unref(frame);
                }
                av_frame_free(&frame);
            } else {
                //                qDebug("Paused or queue empty. Sleeping...");
                usleep(10);
            }
        }
    }
}// namespace AVQt
