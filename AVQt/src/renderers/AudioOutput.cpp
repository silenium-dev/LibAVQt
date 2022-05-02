#include "renderers/AudioOutput.hpp"
#include "private/AudioOutput_p.hpp"

#include "global.hpp"

#include "communication/AudioPadParams.hpp"
#include "communication/Message.hpp"
#include "renderers/AudioOutputFactory.hpp"

#include <pgraph/api/Data.hpp>
#include <pgraph/api/PadUserData.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

namespace AVQt {
    AudioOutput::AudioOutput(QObject *parent)
        : QObject(parent),
          pgraph::impl::SimpleConsumer(pgraph::impl::SimplePadFactory::getInstance()),
          d_ptr(new AudioOutputPrivate(this)) {
    }

    AudioOutput::AudioOutput(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QObject(parent),
          pgraph::impl::SimpleConsumer(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new AudioOutputPrivate(this)) {
    }

    bool AudioOutput::init() {
        Q_D(AudioOutput);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            d->inputPadId = pgraph::impl::SimpleConsumer::createInputPad(pgraph::api::PadUserData::emptyUserData());
            if (d->inputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning() << "AudioOutput: failed to create input pad";
                d->initialized = false;
                return false;
            }
            return true;
        } else {
            qWarning() << "AudioOutput::init() called multiple times";
            return false;
        }
    }

    bool AudioOutput::isOpen() const {
        Q_D(const AudioOutput);
        return d->open;
    }

    bool AudioOutput::isRunning() const {
        Q_D(const AudioOutput);
        return d->running;
    }

    bool AudioOutput::isPaused() const {
        Q_D(const AudioOutput);
        return d->paused;
    }

    void AudioOutput::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(AudioOutput);
        if (data->getType() == communication::Message::Type) {
            auto message = std::dynamic_pointer_cast<communication::Message>(data);
            switch (message->getAction()) {
                case communication::Message::Action::INIT:
                    d->audioParams = message->getPayload("audioParams").value<communication::AudioPadParams>();
                    if (!open()) {
                        qWarning() << "AudioOutput::consume() failed to open";
                    }
                    break;
                case communication::Message::Action::CLEANUP:
                    close();
                    break;
                case communication::Message::Action::START:
                    if (!start()) {
                        qWarning() << "AudioOutput::consume() failed to start";
                    }
                    break;
                case communication::Message::Action::STOP:
                    stop();
                    break;
                case communication::Message::Action::PAUSE:
                    pause(message->getPayload("state").toBool());
                    break;
                case communication::Message::Action::RESET:
                    if (d->impl) {
                        d->impl->resetBuffer();
                    }
                    break;
                case communication::Message::Action::DATA:
                    if (d->impl && !d->paused) {
                        // TODO: Frame timing and synchronization with an external clock
                        d->impl->write(message->getPayload("frame").value<std::shared_ptr<AVFrame>>());
                    }
                    break;
                case communication::Message::Action::RESIZE:
                case communication::Message::Action::NONE:
                    break;
            }
        }
    }

    bool AudioOutput::open() {
        Q_D(AudioOutput);

        if (!d->initialized) {
            qWarning() << "AudioOutput::open() called before init()";
            return false;
        }

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            d->impl = AVQt::AudioOutputFactory::getInstance().create(d->audioParams->format);
            if (!d->impl) {
                qWarning() << "AudioOutput::open() failed to create implementation";
                d->open = false;
                return false;
            }
            return true;
        } else {
            qWarning() << "AudioOutput::open() called multiple times";
            return false;
        }
    }

    void AudioOutput::close() {
        Q_D(AudioOutput);

        if (d->running) {
            stop();
        }

        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            d->impl.reset();
        } else {
            qWarning() << "AudioOutput::close() called multiple times";
        }
    }

    bool AudioOutput::start() {
        Q_D(AudioOutput);

        if (!d->open) {
            qWarning() << "AudioOutput::start() called before open()";
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            if (!d->impl->open(d->audioParams.value())) {
                qWarning() << "AudioOutput::start() failed to start";
                d->running = false;
                return false;
            }
            emit started();
            return true;
        } else {
            qWarning() << "AudioOutput::start() called multiple times";
            return false;
        }
    }

    void AudioOutput::stop() {
        Q_D(AudioOutput);

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->impl->close();
            emit stopped();
        } else {
            qWarning() << "AudioOutput::stop() called multiple times";
        }
    }

    void AudioOutput::pause(bool state) {
        Q_D(AudioOutput);

        if (d->running) {
            bool shouldBe = !state;
            if (d->paused.compare_exchange_strong(shouldBe, state)) {
                d->impl->pause(state);
                emit paused(state);
            } else {
                qWarning() << "AudioOutput::pause() called multiple times";
            }
        }
    }

    AudioOutputPrivate::AudioOutputPrivate(AudioOutput *q) : q_ptr(q) {}
}// namespace AVQt
