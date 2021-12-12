#include "communication/IComponent.hpp"
#include "decoder/IDecoderImpl.hpp"

#include <QtCore>
#include <QtGui>
#include <pgraph/impl/SimpleProcessor.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}


#ifndef TRANSCODE_DECODERVAAPI_H
#define TRANSCODE_DECODERVAAPI_H

namespace AVQt {
    class DecoderPrivate;

    class Decoder : public QThread, public api::IComponent, public pgraph::impl::SimpleProcessor {
        Q_OBJECT

        Q_DECLARE_PRIVATE(AVQt::Decoder)

    public:
        explicit Decoder(const QString &decoderName, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);

        /*!
         * \private
         */
        Decoder(const Decoder &) = delete;

        /*!
         * \private
         */
        void operator=(const Decoder &) = delete;
        bool isOpen() const override;
        bool isRunning() const override;
        /*!
         * \private
         */
        ~Decoder() Q_DECL_OVERRIDE;

        /*!
         * \brief Returns paused state of decoder
         * @return Paused state
         */
        bool isPaused() const Q_DECL_OVERRIDE;

        [[nodiscard]] uint32_t getInputPadId() const;

        [[nodiscard]] uint32_t getOutputPadId() const;

    public slots:
        /*!
         * \brief Initialize decoder. Creates input context, output pad.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE bool open() Q_DECL_OVERRIDE;

        Q_INVOKABLE bool init() Q_DECL_OVERRIDE;

        /*!
         * \brief Clean up decoder. Closes input device, destroys input contexts.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE void close() Q_DECL_OVERRIDE;

        /*!
         * \brief Starts decoder. Sets running flag and starts decoding thread.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE bool start() Q_DECL_OVERRIDE;

        /*!
         * \brief Stops decoder. Resets running flag and interrupts decoding thread.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE void stop() Q_DECL_OVERRIDE;

        /*!
         * \brief Sets paused flag. When set to true, decoder thread will suspend after processing current frame
         * @param pause New paused flag state
         */
        Q_INVOKABLE void pause(bool pause) Q_DECL_OVERRIDE;

        void consume(uint32_t pad, std::shared_ptr<pgraph::api::Data> data) Q_DECL_OVERRIDE;

    signals:

        /*!
         * \brief Emitted after start() finished
         */
        void started() Q_DECL_OVERRIDE;

        /*!
         * \brief Emitted after stop() finished
         */
        void stopped() Q_DECL_OVERRIDE;

        /*!
         * \brief Emitted when paused state changes
         * @param pause Current paused state
         */
        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        /*!
         * \private
         */
        void run() Q_DECL_OVERRIDE;

        /*!
         * \private
         */
        DecoderPrivate *d_ptr;
    };

}// namespace AVQt

Q_DECLARE_METATYPE(AVFrame *)

#endif//TRANSCODE_DECODERVAAPI_H
