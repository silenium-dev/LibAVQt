//
// Created by silas on 27/03/2022.
//

#pragma clang diagnostic push
#pragma ide diagnostic ignored "NotImplementedFunctions"
#ifndef LIBAVQT_MUXER_HPP
#define LIBAVQT_MUXER_HPP

#include "AVQt/communication/IComponent.hpp"
#include "AVQt/communication/PacketPadParams.hpp"

#include <pgraph/api/Data.hpp>
#include <pgraph/impl/SimpleConsumer.hpp>

#include <pgraph_network/api/PadRegistry.hpp>

#include <QThread>

extern "C" {
#include <libavformat/avformat.h>
}

namespace AVQt {
    class MuxerPrivate;
    class Muxer : public QThread, public api::IComponent, public pgraph::impl::SimpleConsumer {
        Q_OBJECT
        Q_DECLARE_PRIVATE(Muxer)
    public:
        struct Config {
            /**
             * @brief any container format supported by libavformat, format specific limitations apply (e.g. mp3 cannot be used for video)
             */
            const char *containerFormat;

            /**
             * @brief An output device, must be writable, but not necessarily seekable. Muxer takes ownership of the pointer.
             *
             * @note The device will be closed when the muxer is destroyed.
             */
            std::unique_ptr<QIODevice> outputDevice;
        };

        explicit Muxer(Config config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        ~Muxer() Q_DECL_OVERRIDE;

        [[nodiscard]] bool isOpen() const Q_DECL_OVERRIDE;
        [[nodiscard]] bool isRunning() const Q_DECL_OVERRIDE;
        [[nodiscard]] bool isPaused() const Q_DECL_OVERRIDE;
        bool init() Q_DECL_OVERRIDE;

        [[maybe_unused]] int64_t createStreamPad();
        [[maybe_unused]] void destroyStreamPad(int64_t padId);

        void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) override;

    protected:
        bool open() Q_DECL_OVERRIDE;
        void close() Q_DECL_OVERRIDE;
        bool start() Q_DECL_OVERRIDE;
        void stop() Q_DECL_OVERRIDE;
        void pause(bool state) Q_DECL_OVERRIDE;

        void run() Q_DECL_OVERRIDE;

    signals:
        void started() Q_DECL_OVERRIDE;
        void stopped() Q_DECL_OVERRIDE;
        void paused(bool state) Q_DECL_OVERRIDE;

    protected:
        [[maybe_unused]] explicit Muxer(Config config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, MuxerPrivate *p, QObject *parent = nullptr);
        QScopedPointer<MuxerPrivate> d_ptr;
    };
}// namespace AVQt
#endif//LIBAVQT_MUXER_HPP

#pragma clang diagnostic pop
