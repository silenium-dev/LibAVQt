#include "Qt6AudioOutputImpl.hpp"
#include "private/Qt6AudioOutputImpl_p.hpp"

#include "renderers/AudioOutputFactory.hpp"

#include <QTimer>
#include <static_block.hpp>

namespace AVQt {
    const api::AudioOutputImplInfo Qt6AudioOutputImpl::info() {// NOLINT(readability-const-return-type)
        api::AudioOutputImplInfo info{
                .name = "Qt6Multimedia",
                .metaObject = Qt6AudioOutputImpl::staticMetaObject,
                .supportedPlatforms = {common::Platform::All},
                .supportedFormats = {

                },
                .isSupported = [] {
                    return true;
                },
        };
        return info;
    }

    template<typename SourceType, typename DestType>
    static QByteArray interleavePlanar(const std::shared_ptr<AVFrame> &frame) {
        QByteArray result{static_cast<qsizetype>(frame->nb_samples * frame->channels * sizeof(DestType)), 0};
        auto *dst = reinterpret_cast<DestType *>(result.data());
        auto *src = reinterpret_cast<SourceType **>(frame->data);
        for (int i = 0; i < frame->nb_samples; ++i) {
            for (int j = 0; j < frame->channels; ++j) {
                dst[i * frame->channels + j] = static_cast<DestType>(src[j][i]);
            }
        }
        return result;
    }

    Qt6AudioOutputImpl::Qt6AudioOutputImpl(QObject *parent)
        : QThread(parent),
          d_ptr(new Qt6AudioOutputImplPrivate(this)) {
    }

    bool Qt6AudioOutputImpl::open(const communication::AudioPadParams &params) {
        Q_D(Qt6AudioOutputImpl);

        if (isRunning()) {
            qWarning() << "Qt6AudioOutputImpl::open: already open";
            return false;
        }

        if (!d->sampleFormatMap.contains(params.format.sampleFormat())) {
            qWarning() << "Unsupported sample format:" << av_get_sample_fmt_name(params.format.sampleFormat());
            return false;
        }
        if (!d->channelLayoutMap.contains(params.format.channelLayout())) {
            char strBuf[256];
            av_get_channel_layout_string(strBuf, sizeof(strBuf), params.format.channels(), params.format.channelLayout());
            qWarning() << "Unsupported channel layout:" << strBuf;
            return false;
        }
        QAudioFormat format{};
        format.setSampleFormat(d->sampleFormatMap.value(params.format.sampleFormat()));
        format.setChannelCount(params.format.channels());
        format.setSampleRate(params.format.sampleRate());
        format.setChannelConfig(d->channelLayoutMap.value(params.format.channelLayout()));

        d->audioFormat = params.format;
        d->audioSink = std::make_unique<QAudioSink>(format, this);
        d->audioSink->setBufferSize(Qt6AudioOutputImplPrivate::bufferSize);
        d->audioSink->setVolume(2.0);
        d->audioOutputDevice = d->audioSink->start();

        start();

        return d->audioOutputDevice != nullptr;
    }

    void Qt6AudioOutputImpl::close() {
        Q_D(Qt6AudioOutputImpl);
        if (isRunning()) {
            quit();
            wait();
            d->audioSink->stop();
            d->audioSink.reset();
            d->audioOutputDevice = nullptr;
        }
    }

    void Qt6AudioOutputImpl::resetBuffer() {
        Q_D(Qt6AudioOutputImpl);
        std::unique_lock lock(d->inputMutex);
        d->inputBuffer.clear();
    }

    void Qt6AudioOutputImpl::pause(bool state) {
        Q_D(Qt6AudioOutputImpl);
        if (!state && d->audioSink->state() == QAudio::SuspendedState) {
            QMetaObject::invokeMethod(d->audioOutputTimer.get(), "start", Q_ARG(int, 2));
            d->audioSink->resume();
            d->inputCond.notify_all();
        } else if (state && d->audioSink->state() == QAudio::ActiveState) {
            QMetaObject::invokeMethod(d->audioOutputTimer.get(), "stop");
            d->audioSink->suspend();
        }
    }

    void Qt6AudioOutputImpl::write(const std::shared_ptr<AVFrame> &frame) {
        Q_D(Qt6AudioOutputImpl);
        if (d->audioOutputDevice == nullptr) {
            return;
        }

        if (d->audioFormat->sampleFormat() != frame->format) {
            qWarning() << "Unsupported sample format:" << av_get_sample_fmt_name(static_cast<AVSampleFormat>(frame->format));
            return;
        }

        if (d->audioFormat->channelLayout() != frame->channel_layout) {
            char strBuf[256];
            av_get_channel_layout_string(strBuf, sizeof(strBuf), frame->channels, frame->channel_layout);
            qWarning() << "Unsupported channel layout:" << strBuf;
            return;
        }

        if (d->audioFormat->sampleRate() != frame->sample_rate) {
            qWarning() << "Unsupported sample rate:" << frame->sample_rate;
            return;
        }

        if (d->audioFormat->channels() != frame->channels) {
            qWarning() << "Unsupported channel count:" << frame->channels;
            return;
        }

        if (!d->sampleFormatMap.contains(static_cast<AVSampleFormat>(frame->format))) {
            qWarning() << "Unsupported sample format:" << av_get_sample_fmt_name(static_cast<AVSampleFormat>(frame->format));
            return;
        }

        QByteArray data;

        if (d->planarFormats.contains(frame->format)) {
            switch (frame->format) {
                case AV_SAMPLE_FMT_U8P:
                    data = interleavePlanar<uint8_t, uint8_t>(frame);
                    break;
                case AV_SAMPLE_FMT_S16P:
                    data = interleavePlanar<int16_t, int16_t>(frame);
                    break;
                case AV_SAMPLE_FMT_S32P:
                    data = interleavePlanar<int32_t, int32_t>(frame);
                    break;
                case AV_SAMPLE_FMT_FLTP:
                    data = interleavePlanar<float, float>(frame);
                    break;
                case AV_SAMPLE_FMT_DBLP:
                    data = interleavePlanar<double, float>(frame);
                    break;
                default:
                    qWarning() << "Unsupported planar format:" << av_get_sample_fmt_name(static_cast<AVSampleFormat>(frame->format));
                    return;
            }
        } else {
            data = QByteArray{reinterpret_cast<const char *>(frame->data[0]), frame->linesize[0]};
        }

        std::unique_lock lock(d->inputMutex);
        if (d->inputBuffer.size() >= Qt6AudioOutputImplPrivate::bufferSize) {
            d->inputCond.notify_one();
            d->inputCond.wait(lock, [d, this] { return d->inputBuffer.size() < Qt6AudioOutputImplPrivate::bufferSize || !this->isRunning(); });
            if (!this->isRunning() || d->inputBuffer.size() >= Qt6AudioOutputImplPrivate::bufferSize) {
                return;
            }
        }
        d->inputBuffer.append(data);
    }

    void Qt6AudioOutputImpl::run() {
        Q_D(Qt6AudioOutputImpl);

        d->audioOutputTimer = std::make_unique<QTimer>();
        connect(d->audioOutputTimer.get(), &QTimer::timeout, d, &Qt6AudioOutputImplPrivate::trySendData, Qt::DirectConnection);
        d->audioOutputTimer->start(2);

        exec();
        d->audioOutputTimer->stop();
        d->audioOutputTimer.reset();
    }

    Qt6AudioOutputImplPrivate::Qt6AudioOutputImplPrivate(Qt6AudioOutputImpl *q) : q_ptr(q) {}

    void Qt6AudioOutputImplPrivate::trySendData() {
        Q_Q(Qt6AudioOutputImpl);

        std::unique_lock lock(inputMutex);
        if (inputBuffer.isEmpty()) {
            inputCond.notify_one();
            inputCond.wait(lock, [this, q] { return !inputBuffer.isEmpty() || !q->isRunning(); });
            if (!q->isRunning() || inputBuffer.isEmpty()) {
                return;
            }
        }
        auto bytesFree = audioSink->bytesFree();
        if (bytesFree) {
            auto bytesToSend = qMin(bytesFree, inputBuffer.size());
            auto lenSent = audioOutputDevice->write(inputBuffer.constData(), bytesToSend);
            if (lenSent) {
                inputBuffer.remove(0, lenSent);
            }
        }
        inputCond.notify_one();
    }
}// namespace AVQt

static_block {
    AVQt::AudioOutputFactory::getInstance().registerOutput(AVQt::Qt6AudioOutputImpl::info());
}
