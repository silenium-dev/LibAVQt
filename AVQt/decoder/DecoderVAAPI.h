#include "communication/IProcessor.hpp"

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
    class DecoderVAAPIPrivate;

    class DecoderVAAPI : public QThread, public IProcessor, public pgraph::impl::SimpleProcessor {
        Q_OBJECT

        Q_DECLARE_PRIVATE(AVQt::DecoderVAAPI)

    public:
        explicit DecoderVAAPI(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);

        /*!
         * \private
         */
        DecoderVAAPI(const DecoderVAAPI &) = delete;

        /*!
         * \private
         */
        void operator=(const DecoderVAAPI &) = delete;

        /*!
         * \private
         */
        ~DecoderVAAPI() Q_DECL_OVERRIDE;

        /*!
         * \brief Returns paused state of decoder
         * @return Paused state
         */
        bool isPaused() Q_DECL_OVERRIDE;

        [[nodiscard]] uint32_t getInputPadId() const Q_DECL_OVERRIDE;

        [[nodiscard]] uint32_t getCommandOutputPadId() const Q_DECL_OVERRIDE;

    public slots:
        /*!
         * \brief Initialize decoder. Creates input contexts, output pads.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE bool open() Q_DECL_OVERRIDE;

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
        DecoderVAAPIPrivate *d_ptr;
    };

}// namespace AVQt

#endif//TRANSCODE_DECODERVAAPI_H
