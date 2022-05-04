#include "Qt5AudioOutputImpl.hpp"
#include "private/Qt5AudioOutputImpl_p.hpp"

#include "renderers/AudioOutputFactory.hpp"

#include <QCoreApplication>
#include <QTimer>
#include <static_block.hpp>

namespace AVQt {
    const api::AudioOutputImplInfo Qt5AudioOutputImpl::info() {// NOLINT(readability-const-return-type)
        api::AudioOutputImplInfo info{
                .name = "Qt5Multimedia",
                .metaObject = Qt5AudioOutputImpl::staticMetaObject,
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
        QByteArray result{static_cast<int>(frame->nb_samples * frame->channels * sizeof(DestType)), 0};
        auto *dst = reinterpret_cast<DestType *>(result.data());
        auto *src = reinterpret_cast<SourceType **>(frame->data);
        for (int i = 0; i < frame->nb_samples; ++i) {
            for (int j = 0; j < frame->channels; ++j) {
                dst[i * frame->channels + j] = static_cast<DestType>(src[j][i]);
            }
        }
        return result;
    }

    Qt5AudioOutputImpl::Qt5AudioOutputImpl(QObject *parent)
        : QThread(parent),
          d_ptr(new Qt5AudioOutputImplPrivate(this)) {
    }

    bool Qt5AudioOutputImpl::open(const communication::AudioPadParams &params) {
        Q_D(Qt5AudioOutputImpl);

        if (isRunning()) {
            qWarning() << "Qt5AudioOutputImpl::open(): already open";
            return false;
        }

        if (!d->sampleFormatMap.contains(params.format.sampleFormat())) {
            qWarning() << "Qt5AudioOutputImpl::open(): Unsupported sample format:" << av_get_sample_fmt_name(params.format.sampleFormat());
            return false;
        }

        QAudioFormat format{};
        format.setSampleType(d->sampleFormatMap.value(params.format.sampleFormat()));
        format.setChannelCount(params.format.channels());
        format.setSampleRate(params.format.sampleRate());
        format.setSampleSize(av_get_bytes_per_sample(params.format.sampleFormat()) * 8);
        format.setCodec("audio/pcm");
        format.setByteOrder(QAudioFormat::LittleEndian);

        QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
        if (!info.isFormatSupported(format)) {
            qWarning() << "Qt5AudioOutputImpl::open(): Raw audio format not supported by backend, cannot play audio.";
            return false;
        }

        d->audioFormat = params.format;
        d->audioOutput = std::make_unique<QAudioOutput>(QAudioDeviceInfo::defaultOutputDevice(), format, this);
        d->audioOutput->setBufferSize(Qt5AudioOutputImplPrivate::bufferSize);
        d->audioOutput->setVolume(2.0);
        d->audioOutputDevice = d->audioOutput->start();

        start();

        return d->audioOutputDevice != nullptr;
    }

    void Qt5AudioOutputImpl::close() {
        Q_D(Qt5AudioOutputImpl);
        if (isRunning()) {
            quit();
            wait();
            d->audioOutput->stop();
            d->audioOutput.reset();
            d->audioOutputDevice = nullptr;
        }
    }

    void Qt5AudioOutputImpl::resetBuffer() {
        Q_D(Qt5AudioOutputImpl);
        std::unique_lock lock(d->inputMutex);
        d->inputBuffer.clear();
    }

    void Qt5AudioOutputImpl::pause(bool state) {
        Q_D(Qt5AudioOutputImpl);
        if (!state && d->audioOutput->state() == QAudio::SuspendedState) {
            QMetaObject::invokeMethod(d->audioOutputTimer.get(), "start", Q_ARG(int, 2));
            d->audioOutput->resume();
            d->inputCond.notify_all();
        } else if (state && d->audioOutput->state() == QAudio::ActiveState) {
            QMetaObject::invokeMethod(d->audioOutputTimer.get(), "stop");
            d->audioOutput->suspend();
        }
    }

    void Qt5AudioOutputImpl::write(const std::shared_ptr<AVFrame> &frame) {
        Q_D(Qt5AudioOutputImpl);
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

        if (d->planarFormats.contains(static_cast<AVSampleFormat>(frame->format))) {
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
        if (d->inputBuffer.size() >= Qt5AudioOutputImplPrivate::bufferSize) {
            d->inputCond.notify_one();
            d->inputCond.wait(lock, [d, this] { return d->inputBuffer.size() < Qt5AudioOutputImplPrivate::bufferSize || !this->isRunning(); });
            if (!this->isRunning() || d->inputBuffer.size() >= Qt5AudioOutputImplPrivate::bufferSize) {
                return;
            }
        }
        d->inputBuffer.append(data);
    }

    void Qt5AudioOutputImpl::run() {
        Q_D(Qt5AudioOutputImpl);

        d->audioOutputTimer = std::make_unique<QTimer>();
        connect(d->audioOutputTimer.get(), &QTimer::timeout, d, &Qt5AudioOutputImplPrivate::trySendData, Qt::DirectConnection);
        d->audioOutputTimer->start(2);

        exec();
        d->audioOutputTimer->stop();
        d->audioOutputTimer.reset();
    }

    Qt5AudioOutputImplPrivate::Qt5AudioOutputImplPrivate(Qt5AudioOutputImpl *q) : q_ptr(q) {}

    void Qt5AudioOutputImplPrivate::trySendData() {
        Q_Q(Qt5AudioOutputImpl);

        std::unique_lock lock(inputMutex);
        if (inputBuffer.isEmpty()) {
            inputCond.notify_one();
            inputCond.wait(lock, [this, q] { return !inputBuffer.isEmpty() || !q->isRunning(); });
            if (!q->isRunning() || inputBuffer.isEmpty()) {
                return;
            }
        }
        auto bytesFree = audioOutput->bytesFree();
        if (bytesFree) {
            auto bytesToSend = qMin(bytesFree, inputBuffer.size());
            auto lenSent = audioOutputDevice->write(inputBuffer.constData(), bytesToSend);
            if (lenSent) {
                inputBuffer.remove(0, static_cast<int>(lenSent));
            }
        }
        inputCond.notify_one();
    }
}// namespace AVQt

static_block {
    AVQt::AudioOutputFactory::getInstance().registerOutput(AVQt::Qt5AudioOutputImpl::info());
}
