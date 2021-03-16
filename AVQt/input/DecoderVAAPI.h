//
// Created by silas on 3/1/21.
//

#include "IFrameSource.h"

#include <QtCore>
#include <QtGui>

extern "C" {
#include <libavutil/avutil.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}


#ifndef TRANSCODE_DECODERVAAPI_H
#define TRANSCODE_DECODERVAAPI_H

namespace AVQt {
    struct DecoderVAAPIPrivate;

    /*!
     * \class DecoderVAAPI
     * \brief VAAPI accelerated video decoder
     *
     * Decodes video from QIODevice into single frames, that are passed to every via registerCallback registered callback.
     * It does ***not*** stop when no callbacks are registered, so make sure,
     * that either at least one callback is registered or the decoder is paused, or frames will be dropped
     */
    class DecoderVAAPI : public QThread, public IFrameSource {
    Q_OBJECT
        Q_INTERFACES(AVQt::IFrameSource)

        Q_DECLARE_PRIVATE(AVQt::DecoderVAAPI)

    public:
        /*!
         * Standard public constructor. Creates new instance with given input device. This device can not be changed afterwards,
         * because the decoder would have to be completely reinitialized thereafter
         * @param inputDevice QIODevice to read video from
         * @param parent Pointer to parent QObject for Qt's meta object system
         */
        explicit DecoderVAAPI(QIODevice *inputDevice, QObject *parent = nullptr);

        /*!
         * \private
         */
        ~DecoderVAAPI() override = default;

        /*!
         * \brief Returns paused state of decoder
         * @return Paused state
         */
        bool isPaused() override;

    public slots:
        /*!
         * \brief Initialize decoder. Opens input device, creates input contexts.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int init() override;

        /*!
         * \brief Clean up decoder. Closes input device, destroys input contexts.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int deinit() override;

        /*!
         * \brief Starts decoder. Sets running flag and starts decoding thread.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int start() override;

        /*!
         * \brief Stops decoder. Resets running flag and interrupts decoding thread.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int stop() override;

        /*!
         * \brief Sets paused flag. When set to true, decoder thread will suspend after processing current frame
         * @param pause New paused flag state
         */
        Q_INVOKABLE void pause(bool pause) override;

        /*!
         * \brief Register frame sink/filter with given callback type \c type
         * @param frameSink Frame sink/filter to be added
         * @param type One element or some bitwise or combination of elements of IFrameSource::CB_TYPE
         * @return Current position in callback list
         */
        Q_INVOKABLE int registerCallback(IFrameSink *frameSink, uint8_t type) override;

        /*!
         * \brief Removes frame sink/filter from registry
         * @param frameSink Frame sink/filter to be removed
         * @return Last position in callback list, -1 when not found
         */
        Q_INVOKABLE int unregisterCallback(IFrameSink *frameSink) override;

    signals:

        /*!
         * \brief Emitted after start() finished
         */
        void started() override;

        /*!
         * \brief Emitted after stop() finished
         */
        void stopped() override;

        /*!
         * \brief Emitted when paused state changes
         * @param pause Current paused state
         */
        void paused(bool pause) override;

    protected:
        /*!
         * Protected constructor for use in derived classes to initialize private struct
         * @param p Private struct
         */
        explicit DecoderVAAPI(DecoderVAAPIPrivate &p);

        /*!
         * \private
         */
        void run() override;

        /*!
         * \private
         */
        DecoderVAAPIPrivate *d_ptr;
    };

}

#endif //TRANSCODE_DECODERVAAPI_H
