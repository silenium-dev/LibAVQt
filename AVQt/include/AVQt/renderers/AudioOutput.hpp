#ifndef LIBAVQT_AUDIOOUTPUT_HPP
#define LIBAVQT_AUDIOOUTPUT_HPP

#include "AVQt/communication/IComponent.hpp"

#include <pgraph/impl/SimpleConsumer.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

#include <QtCore/QObject>

namespace AVQt {
    class AudioOutputPrivate;
    class AudioOutput : public QObject, public pgraph::impl::SimpleConsumer, public api::IComponent {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IComponent)
        Q_DECLARE_PRIVATE(AudioOutput)
    public:
        explicit AudioOutput(QObject *parent = nullptr);
        explicit AudioOutput(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        ~AudioOutput() override = default;

        bool init() override;

        bool isOpen() const override;
        bool isRunning() const override;
        bool isPaused() const override;

        void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) override;

    signals:
        void started() override;
        void stopped() override;
        void paused(bool state) override;

    protected:
        bool open() override;
        void close() override;
        bool start() override;
        void stop() override;
        void pause(bool state) override;

    private:
        std::shared_ptr<AudioOutputPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_AUDIOOUTPUT_HPP
