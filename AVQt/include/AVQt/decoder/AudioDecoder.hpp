#ifndef LIBAVQT_AUDIODECODER_HPP
#define LIBAVQT_AUDIODECODER_HPP

#include "AVQt/communication/IComponent.hpp"

#include <pgraph/impl/SimpleProcessor.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

#include <QtCore/QThread>


namespace AVQt {
    class AudioDecoderPrivate;
    class AudioDecoder : public QThread, public pgraph::impl::SimpleProcessor, public api::IComponent {
        Q_OBJECT
        Q_DECLARE_PRIVATE(AudioDecoder)
        Q_DISABLE_COPY_MOVE(AudioDecoder)
    public:
        struct Config {
            QStringList decoderPriority{};
        };

        explicit AudioDecoder(const Config &config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        explicit AudioDecoder(const Config &config, QObject *parent = nullptr);

        void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) Q_DECL_OVERRIDE;

        bool isOpen() const Q_DECL_OVERRIDE;
        bool isRunning() const Q_DECL_OVERRIDE;
        bool isPaused() const Q_DECL_OVERRIDE;
        bool init() Q_DECL_OVERRIDE;

    signals:
        void started() Q_DECL_OVERRIDE;
        void stopped() Q_DECL_OVERRIDE;
        void paused(bool state) Q_DECL_OVERRIDE;

    protected:
        bool open() Q_DECL_OVERRIDE;
        void close() Q_DECL_OVERRIDE;
        bool start() Q_DECL_OVERRIDE;
        void stop() Q_DECL_OVERRIDE;
        void pause(bool state) Q_DECL_OVERRIDE;

        void run() Q_DECL_OVERRIDE;

    protected:
        std::shared_ptr<AudioDecoderPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_AUDIODECODER_HPP
