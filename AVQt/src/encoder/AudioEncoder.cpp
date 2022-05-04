#include "encoder/AudioEncoder.hpp"
#include "private/AudioEncoder_p.hpp"

#include "global.hpp"

#include "communication/Message.hpp"
#include "encoder/AudioEncoderFactory.hpp"

#include <pgraph/impl/SimplePadFactory.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>
#include <utility>

namespace AVQt {
    AudioEncoder::AudioEncoder(const Config &config, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::impl::SimplePadFactory::getInstance()),
          d_ptr(new AudioEncoderPrivate(config, this)) {
    }

    AudioEncoder::AudioEncoder(const AudioEncoder::Config &config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new AudioEncoderPrivate(config, this)) {
    }

    AudioEncoder::~AudioEncoder() = default;

    bool AudioEncoder::isOpen() const {
        Q_D(const AudioEncoder);
        return d->open;
    }

    bool AudioEncoder::isRunning() const {
        Q_D(const AudioEncoder);
        return d->running;
    }

    bool AudioEncoder::isPaused() const {
        Q_D(const AudioEncoder);
        return d->paused;
    }

    bool AudioEncoder::init() {
        Q_D(AudioEncoder);

        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            d->inputPadParams = std::make_shared<communication::AudioPadParams>();
            d->inputPadId = createInputPad(d->inputPadParams);
            if (d->inputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning("AudioEncoder: Failed to create input pad");
                return false;
            }
            d->outputPadParams = std::make_shared<communication::PacketPadParams>();
            d->outputPadId = createOutputPad(d->outputPadParams);
            if (d->outputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning("AudioEncoder: Failed to create output pad");
                return false;
            }
            return true;
        } else {
            qWarning("AudioEncoder: Already initialized");
        }
        return false;
    }

    void AudioEncoder::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(AudioEncoder);

        if (!d->initialized) {
            qWarning("AudioEncoder: Not initialized");
            return;
        }

        if (pad == d->inputPadId) {
            if (data->getType() == communication::Message::Type) {
                auto message = std::static_pointer_cast<communication::Message>(data);
                switch (static_cast<AVQt::communication::Message::Action::Enum>(message->getAction())) {
                    case communication::Message::Action::INIT:
                        d->inputParams = message->getPayload("audioParams").value<communication::AudioPadParams>();
                        if (!open()) {
                            qWarning("AudioEncoder: Failed to open");
                        }
                        break;
                    case communication::Message::Action::CLEANUP:
                        close();
                        break;
                    case communication::Message::Action::START:
                        if (!start()) {
                            qWarning("AudioEncoder: Failed to start");
                        }
                        break;
                    case communication::Message::Action::STOP:
                        stop();
                        break;
                    case communication::Message::Action::PAUSE:
                        pause(message->getPayload("state").toBool());
                        break;
                    case communication::Message::Action::DATA:
                        d->enqueueData(message->getPayload("frame").value<std::shared_ptr<AVFrame>>());
                        break;
                    case communication::Message::Action::RESET: {
                        if (d->open) {
                            std::unique_lock lock{d->inputQueueMutex};
                            d->inputQueueCond.wait(lock, [d] { return d->inputQueue.empty() || !d->running; });
                            d->impl->close();
                            pgraph::impl::SimpleProcessor::produce(communication::Message::builder().withAction(communication::Message::Action::RESET).build(), d->outputPadId);
                            if (!d->impl->open(d->inputParams)) {
                                qWarning("AudioEncoder: Failed to open");
                                close();
                            }
                        }
                        break;
                    }
                    case communication::Message::Action::RESIZE:
                    case communication::Message::Action::NONE:
                        break;
                    default:
                        qFatal("Unimplemented action %s", message->getAction().name().toLocal8Bit().data());
                }
            }
        } else {
            qWarning("AudioEncoder: Unknown pad %ld", pad);
        }
    }

    bool AudioEncoder::open() {
        Q_D(AudioEncoder);

        if (!d->initialized) {
            qWarning("AudioEncoder: Not initialized");
            return false;
        }

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            d->impl = AVQt::AudioEncoderFactory::getInstance().create(d->inputParams.format, getAudioCodecId(d->config.codec), d->config.encodeParameters, d->config.encoderPriority);

            if (!d->impl) {
                qFatal("No AudioEncoderImpl found");
            }

            if (!d->impl->open(d->inputParams)) {
                d->impl.reset();
                qWarning("AudioEncoder: Failed to open");
                return false;
            }

            connect(std::dynamic_pointer_cast<QObject>(d->impl).get(), SIGNAL(packetReady(std::shared_ptr<AVPacket>)),
                    this, SLOT(onPacketReady(std::shared_ptr<AVPacket>)), Qt::DirectConnection);

            *d->outputPadParams = *d->impl->getPacketPadParams();

            pgraph::impl::SimpleProcessor::produce(communication::Message::builder()
                                                           .withAction(communication::Message::Action::INIT)
                                                           .withPayload("packetParams", QVariant::fromValue(std::const_pointer_cast<const communication::PacketPadParams>(d->outputPadParams)))
                                                           .withPayload("encodeParams", QVariant::fromValue(d->config.encodeParameters))
                                                           .build(),
                                                   d->outputPadId);

            return true;
        } else {
            qWarning("AudioEncoder: Already open");
        }
        return false;
    }

    void AudioEncoder::close() {
        Q_D(AudioEncoder);

        if (!d->initialized) {
            qWarning("AudioEncoder: Not initialized");
            return;
        }

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
            d->codecParams.reset();
        } else {
            qWarning("AudioEncoder: Not open");
        }
    }

    bool AudioEncoder::start() {
        Q_D(AudioEncoder);

        if (!d->initialized) {
            qWarning("AudioEncoder: Not initialized");
            return false;
        }

        if (!d->open) {
            qWarning("AudioEncoder: Not open");
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->paused = false;
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::START)
                            .build(),
                    d->outputPadId);
            QThread::start();
            return true;
        } else {
            qWarning("AudioEncoder: Already running");
        }
        return false;
    }

    void AudioEncoder::stop() {
        Q_D(AudioEncoder);
        if (!d->initialized) {
            qWarning("AudioEncoder: Not initialized");
            return;
        }

        if (!d->open) {
            qWarning("AudioEncoder: Not open");
            return;
        }

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = false;
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::STOP)
                            .build(),
                    d->outputPadId);
            d->inputQueueCond.notify_all();
            d->pausedCond.notify_all();
            QThread::quit();
            QThread::wait();
        } else {
            qWarning("AudioEncoder: Not running");
        }
    }

    void AudioEncoder::pause(bool state) {
        Q_D(AudioEncoder);

        if (!d->initialized) {
            qWarning("AudioEncoder: Not initialized");
            return;
        }

        if (!d->open) {
            qWarning("AudioEncoder: Not open");
            return;
        }

        bool shouldBe = !state;
        if (d->paused.compare_exchange_strong(shouldBe, state)) {
            produce(communication::Message::builder()
                            .withAction(communication::Message::Action::PAUSE)
                            .withPayload("state", state)
                            .build(),
                    d->outputPadId);

            std::unique_lock pausedLock(d->pausedMutex);
            d->paused = state;
            d->pausedCond.notify_all();
        } else {
            qWarning("AudioEncoder: Already %s", state ? "paused" : "resumed");
        }
    }

    void AudioEncoder::run() {
        Q_D(AudioEncoder);

        if (!d->running) {
            qWarning("AudioEncoder: Not running");
            return;
        }

        while (d->running) {
            std::unique_lock pausedLock(d->pausedMutex);
            if (d->paused) {
                d->pausedCond.wait(pausedLock, [d] { return !d->paused || !d->running; });
                if (!d->running) {
                    break;
                }
            }
            pausedLock.unlock();

            std::unique_lock lock(d->inputQueueMutex);
            if (d->inputQueue.empty()) {
                d->inputQueueCond.wait(lock, [&] { return !d->inputQueue.empty() || !d->running; });
                if (!d->running) {
                    break;
                }
            }

            auto frame = d->inputQueue.front();
            lock.unlock();

            if (d->inputParams.format.sampleFormat() != frame->format ||
                d->inputParams.format.sampleRate() != frame->sample_rate ||
                d->inputParams.format.channelLayout() != frame->channel_layout ||
                d->inputParams.format.channels() != frame->channels) {
                qWarning("AudioEncoder: Input format mismatch");
                continue;
            }

            int ret = d->impl->encode(frame);
            if (ret != EXIT_SUCCESS && ret != EAGAIN && ret < 0) {
                char strBuf[AV_ERROR_MAX_STRING_SIZE];
                qWarning("AudioEncoder: Failed to encode frame: %s", av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret)));
            } else {
                lock.lock();
                d->inputQueue.pop();
                d->inputQueueCond.notify_all();
                lock.unlock();
            }
        }
    }

    void AudioEncoder::onPacketReady(const std::shared_ptr<AVPacket> &packet) {
        Q_D(AudioEncoder);
        produce(communication::Message::builder().withAction(communication::Message::Action::DATA).withPayload("packet", QVariant::fromValue(packet)).build(), d->outputPadId);
    }

    AudioEncoderPrivate::AudioEncoderPrivate(AudioEncoder::Config config, AudioEncoder *q)
        : q_ptr(q), config(std::move(config)) {
    }

    void AudioEncoderPrivate::enqueueData(std::shared_ptr<AVFrame> frame) {
        Q_Q(AudioEncoder);

        std::unique_lock lock(inputQueueMutex);
        if (inputQueue.size() >= inputQueueMaxSize) {
            inputQueueCond.notify_all();
            inputQueueCond.wait(lock, [this] { return inputQueue.size() < inputQueueMaxSize || !running; });
            if (inputQueue.size() >= inputQueueMaxSize) {
                qWarning("AudioEncoder: Input queue is full");
                return;
            }
        }

        inputQueue.push(std::move(frame));
        inputQueueCond.notify_all();
    }
}// namespace AVQt
