#include "AVQt/decoder/AudioDecoder.hpp"
#include "private/AudioDecoder_p.hpp"

#include "global.hpp"

#include "communication/AudioPadParams.hpp"
#include "communication/Message.hpp"
#include "communication/PacketPadParams.hpp"
#include "decoder/AudioDecoderFactory.hpp"

#include <pgraph/api/Data.hpp>
#include <pgraph/impl/SimplePadFactory.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

#include <QThread>

namespace AVQt {
    AudioDecoder::AudioDecoder(const Config &config, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::impl::SimplePadFactory::getInstance()),
          d_ptr(new AudioDecoderPrivate(this)) {
        Q_D(AudioDecoder);
        d->init(config);
    }

    AudioDecoder::AudioDecoder(const AudioDecoder::Config &config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new AudioDecoderPrivate(this)) {
        Q_D(AudioDecoder);
        d->init(config);
    }

    bool AudioDecoder::init() {
        Q_D(AudioDecoder);

        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            d->inputPadParams = std::make_shared<communication::PacketPadParams>();
            d->inputPadParams->mediaType = AVMEDIA_TYPE_AUDIO;

            d->outputPadParams = std::make_shared<communication::AudioPadParams>(common::AudioFormat{0, 0, AV_SAMPLE_FMT_U8});

            d->inputPadId = pgraph::impl::SimpleProcessor::createInputPad(d->inputPadParams);
            d->outputPadId = pgraph::impl::SimpleProcessor::createOutputPad(d->outputPadParams);

            return true;
        } else {
            qWarning() << "AudioDecoder::init() called multiple times";
            return false;
        }
    }

    bool AudioDecoder::isOpen() const {
        Q_D(const AudioDecoder);
        return d->open;
    }

    bool AudioDecoder::isRunning() const {
        Q_D(const AudioDecoder);
        return d->running;
    }

    bool AudioDecoder::isPaused() const {
        Q_D(const AudioDecoder);
        return d->paused;
    }

    void AudioDecoder::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(AudioDecoder);
        if (data->getType() == communication::Message::Type) {
            auto message = std::static_pointer_cast<communication::Message>(data);
            switch (message->getAction()) {
                case communication::Message::Action::INIT: {
                    auto packetParams = message->getPayload("packetParams").value<std::shared_ptr<const communication::PacketPadParams>>();
                    d->codecParams = packetParams->codecParams;
                    if (!open()) {
                        qWarning() << "Failed to open audio decoder";
                        return;
                    }
                    break;
                }
                case communication::Message::Action::CLEANUP:
                    close();
                    break;
                case communication::Message::Action::START:
                    if (!start()) {
                        qWarning() << "Failed to start decoder";
                        return;
                    }
                    break;
                case communication::Message::Action::STOP:
                    stop();
                    break;
                case communication::Message::Action::PAUSE:
                    pause(message->getPayload("state").toBool());
                    break;
                case communication::Message::Action::DATA: {
                    auto packet = message->getPayload("packet").value<std::shared_ptr<AVPacket>>();
                    std::unique_lock lock(d->inputQueueMutex);
                    d->inputQueue.push(std::move(packet));
                    d->inputQueueCond.notify_all();
                    break;
                }
                case communication::Message::Action::RESET: {
                    if (d->open) {
                        std::unique_lock lock(d->inputQueueMutex);
                        if (!d->inputQueue.empty()) {
                            d->inputQueueCond.wait(lock, [d] { return d->inputQueue.empty(); });
                        }
                        d->impl->close();
                        pgraph::impl::SimpleProcessor::produce(communication::Message::builder().withAction(communication::Message::Action::RESET).build(), d->outputPadId);
                        if (!d->impl->open(d->inputPadParams->codecParams)) {
                            qWarning() << "Failed to open audio decoder";
                            close();
                        }
                    }
                    break;
                }
                case communication::Message::Action::RESIZE:
                case communication::Message::Action::NONE:
                    break;
            }
        }
    }

    bool AudioDecoder::open() {
        Q_D(AudioDecoder);

        if (!d->initialized) {
            throw std::runtime_error("AudioDecoder not initialized");
        }

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            common::AudioFormat format{d->codecParams->sample_rate, d->codecParams->channels, static_cast<AVSampleFormat>(d->codecParams->format), d->codecParams->channel_layout};
            d->impl = AudioDecoderFactory::getInstance().create(format, d->codecParams->codec_id, d->config.decoderPriority);
            if (!d->impl) {
                throw std::runtime_error("Failed to create decoder");
            }
            d->impl->open(d->codecParams);
            *d->outputPadParams = d->impl->getAudioParams();
            d->inputPadParams->codecParams = d->codecParams;
            d->inputPadParams->codec = avcodec_find_decoder(d->codecParams->codec_id);

            connect(std::dynamic_pointer_cast<QObject>(d->impl).get(),
                    SIGNAL(frameReady(std::shared_ptr<AVFrame>)),
                    d,
                    SLOT(onFrame(std::shared_ptr<AVFrame>)),
                    Qt::DirectConnection);

            pgraph::impl::SimpleProcessor::produce(communication::Message::builder()
                                                           .withAction(communication::Message::Action::INIT)
                                                           .withPayload("audioParams", QVariant::fromValue(*d->outputPadParams))
                                                           .build(),
                                                   d->outputPadId);
            return true;
        } else {
            qWarning() << "AudioDecoder already open";
            return false;
        }
    }

    void AudioDecoder::close() {
        Q_D(AudioDecoder);

        if (d->running) {
            stop();
        }

        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            pgraph::impl::SimpleProcessor::produce(communication::Message::builder()
                                                           .withAction(communication::Message::Action::CLEANUP)
                                                           .build(),
                                                   d->outputPadId);
            d->impl->close();
            d->impl.reset();
            d->outputPadParams->format = common::AudioFormat{0, 0, AV_SAMPLE_FMT_NONE, 0};
            d->codecParams.reset();
            d->inputPadParams->codecParams.reset();
            d->inputPadParams->codec = nullptr;
        }
    }

    bool AudioDecoder::start() {
        Q_D(AudioDecoder);

        if (!d->open) {
            throw std::runtime_error("AudioDecoder not open");
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->paused = false;
            pgraph::impl::SimpleProcessor::produce(communication::Message::builder()
                                                           .withAction(communication::Message::Action::START)
                                                           .build(),
                                                   d->outputPadId);
            QThread::start();

            emit started();
            return true;
        } else {
            qWarning() << "AudioDecoder already running";
            return false;
        }
    }

    void AudioDecoder::stop() {
        Q_D(AudioDecoder);

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = false;
            pgraph::impl::SimpleProcessor::produce(communication::Message::builder()
                                                           .withAction(communication::Message::Action::STOP)
                                                           .build(),
                                                   d->outputPadId);
            d->pausedCond.notify_all();
            d->inputQueueCond.notify_all();
            QThread::quit();
            QThread::wait();

            std::unique_lock lock(d->inputQueueMutex);
            while (!d->inputQueue.empty()) {
                d->inputQueue.pop();
            }

            emit stopped();
        } else {
            qWarning() << "AudioDecoder already stopped";
        }
    }

    void AudioDecoder::pause(bool state) {
        Q_D(AudioDecoder);
        bool shouldBe = !state;
        if (d->paused.compare_exchange_strong(shouldBe, state)) {
            pgraph::impl::SimpleProcessor::produce(communication::Message::builder()
                                                           .withAction(communication::Message::Action::PAUSE)
                                                           .withPayload("state", state)
                                                           .build(),
                                                   d->outputPadId);
            std::unique_lock pausedLock(d->pausedMutex);
            d->pausedCond.notify_all();
            emit paused(state);
        } else {
            qDebug() << "AudioDecoder::pause: state already" << state;
        }
    }

    void AudioDecoder::run() {
        Q_D(AudioDecoder);

        while (d->running) {
            std::unique_lock lock{d->pausedMutex};
            if (d->paused) {
                d->pausedCond.wait(lock, [d] { return !d->paused || !d->running; });
                if (!d->running) {
                    break;
                }
            }
            lock.unlock();
            std::unique_lock inputLock(d->inputQueueMutex);
            if (d->inputQueue.empty()) {
                d->inputQueueCond.wait(inputLock, [d] { return !d->inputQueue.empty() || !d->running; });
                if (!d->running) {
                    break;
                }
            }
            std::shared_ptr<AVPacket> packet = d->inputQueue.front();
            inputLock.unlock();
            auto ret = d->impl->decode(packet);
            if (ret == EAGAIN) {
                inputLock.lock();
                d->inputQueue.pop();
            } else if (ret != EXIT_SUCCESS) {
                char err[AV_ERROR_MAX_STRING_SIZE];
                qWarning() << "AudioDecoder::run: error decoding packet" << av_make_error_string(err, AV_ERROR_MAX_STRING_SIZE, AVERROR(ret));
                break;
            } else {
                inputLock.lock();
                d->inputQueue.pop();
                d->inputQueueCond.notify_all();
            }
        }
    }

    AudioDecoderPrivate::AudioDecoderPrivate(AudioDecoder *q) : QObject(q), q_ptr(q) {}

    void AudioDecoderPrivate::init(const AudioDecoder::Config &aConfig) {
        Q_Q(AudioDecoder);
        config = aConfig;
    }

    void AudioDecoderPrivate::onFrame(const std::shared_ptr<AVFrame> &frame) {
        Q_Q(AudioDecoder);
        if (!running) {
            return;
        }
        if (frame) {
            auto message = communication::Message::builder().withAction(communication::Message::Action::DATA).withPayload("frame", QVariant::fromValue(frame)).build();
            std::unique_lock pausedLock{pausedMutex};
            if (paused) {
                pausedCond.wait(pausedLock, [this] { return !paused || !running; });
                if (!running) {
                    return;
                }
            }
            q->produce(message, outputPadId);
        }
    }
}// namespace AVQt
