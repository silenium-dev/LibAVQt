#ifndef LIBAVQT_AUDIOENCODER_HPP
#define LIBAVQT_AUDIOENCODER_HPP

#include "AVQt/communication/IComponent.hpp"
#include "AVQt/encoder/IAudioEncoderImpl.hpp"

#include <pgraph/impl/SimpleProcessor.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

#include <QThread>

namespace AVQt {
    class AudioEncoderPrivate;
    class AudioEncoder : public QThread, public pgraph::impl::SimpleProcessor, public api::IComponent {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IComponent)
        Q_DECLARE_PRIVATE(AudioEncoder)
    public:
        struct Config {
            QStringList encoderPriority{};
            AudioCodec codec{};
            AudioEncodeParameters encodeParameters{};
        };

        AudioEncoder(const Config &config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        explicit AudioEncoder(const Config &config, QObject *parent = nullptr);
        ~AudioEncoder() override;

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

        void run() override;

    private slots:
        void onPacketReady(const std::shared_ptr<AVPacket> &packet);

    private:
        std::shared_ptr<AudioEncoderPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_AUDIOENCODER_HPP
