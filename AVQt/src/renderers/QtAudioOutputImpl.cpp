#include "QtAudioOutputImpl.hpp"
#include "private/QtAudioOutputImpl_p.hpp"

#include "renderers/AudioOutputFactory.hpp"

#include <QTimer>
#include <static_block.hpp>

namespace AVQt {
    const api::AudioOutputImplInfo QtAudioOutputImpl::info() {// NOLINT(readability-const-return-type)
        api::AudioOutputImplInfo info{
                .name = "QtMultimedia",
                .metaObject = QtAudioOutputImpl::staticMetaObject,
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

    QtAudioOutputImpl::QtAudioOutputImpl(QObject *parent)
        : QThread(parent),
          d_ptr(new QtAudioOutputImplPrivate(this)) {
    }

    bool QtAudioOutputImpl::open(const communication::AudioPadParams &params) {
        Q_D(QtAudioOutputImpl);

        if (isRunning()) {
            qWarning() << "QtAudioOutputImpl::open: already open";
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
        d->audioSink->setBufferSize(1024);
        d->audioSink->setVolume(2.0);
        d->audioOutputDevice = d->audioSink->start();

        start();

        return d->audioOutputDevice != nullptr;
    }

    void QtAudioOutputImpl::close() {
        Q_D(QtAudioOutputImpl);
        if (isRunning()) {
            quit();
            wait();
            d->audioSink->stop();
            d->audioSink.reset();
            d->audioOutputDevice = nullptr;
        }
    }

    void QtAudioOutputImpl::resetBuffer() {
        Q_D(QtAudioOutputImpl);
        std::unique_lock lock(d->inputMutex);
        d->inputBuffer.clear();
    }

    void QtAudioOutputImpl::write(const std::shared_ptr<AVFrame> &frame) {
        Q_D(QtAudioOutputImpl);
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
        if (d->inputBuffer.size() >= QtAudioOutputImplPrivate::bufferSize) {
            d->inputCond.notify_one();
            d->inputCond.wait(lock, [d, this] { return d->inputBuffer.size() < QtAudioOutputImplPrivate::bufferSize || !this->isRunning(); });
            if (!this->isRunning() || d->inputBuffer.size() >= QtAudioOutputImplPrivate::bufferSize) {
                return;
            }
        }
        d->inputBuffer.append(data);
    }

    void QtAudioOutputImpl::run() {
        Q_D(QtAudioOutputImpl);

        QTimer timer;
        connect(&timer, &QTimer::timeout, d, &QtAudioOutputImplPrivate::trySendData, Qt::DirectConnection);
        timer.start(2);

        exec();
    }

    QtAudioOutputImplPrivate::QtAudioOutputImplPrivate(QtAudioOutputImpl *q) : q_ptr(q) {}

    void QtAudioOutputImplPrivate::trySendData() {
        Q_Q(QtAudioOutputImpl);

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
    AVQt::AudioOutputFactory::getInstance().registerOutput(AVQt::QtAudioOutputImpl::info());
}
