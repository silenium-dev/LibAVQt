#include "IDecoder.h"

#include <QtCore>
#include <QtGui>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}


#ifndef TRANSCODE_DECODERMMAL_H
#define TRANSCODE_DECODERMMAL_H

namespace AVQt {
    class DecoderMMALPrivate;

    /*!
     * \class DecoderMMAL
     * \brief MMAL accelerated video decoder
     *
     * Decodes video from packet source into single frames, that are passed to every via registerCallback registered callback.
     * It does ***not*** stop when no callbacks are registered, so make sure,
     * that either at least one callback is registered or the decoder is paused, or frames will be dropped
     */
    class DecoderMMAL : public QThread, public IDecoder {
    Q_OBJECT
        Q_INTERFACES(AVQt::IDecoder)
//        Q_INTERFACES(AVQt::IFrameSource)
//        Q_INTERFACES(AVQt::IPacketSink)

        Q_DECLARE_PRIVATE(AVQt::DecoderMMAL)

    public:

        /*!
         * Standard public constructor. Creates new instance with given input device. This device can not be changed afterwards,
         * because the decoder would have to be completely reinitialized thereafter
         * @param inputDevice QIODevice to read video from
         * @param parent Pointer to parent QObject for Qt's meta object system
         */
        explicit DecoderMMAL(QObject *parent = nullptr);

        DecoderMMAL(DecoderMMAL &&other) noexcept;

        /*!
         * \private
         */
        DecoderMMAL(const DecoderMMAL &) = delete;

        /*!
         * \private
         */
        void operator=(const DecoderMMAL &) = delete;

        /*!
         * \private
         */
        ~DecoderMMAL() Q_DECL_OVERRIDE;

        /*!
         * \brief Returns paused state of decoder
         * @return Paused state
         */
        bool isPaused() Q_DECL_OVERRIDE;

        /*!
         * \brief Register frame sink/filter with given callback type \c type
         * @param frameSink Frame sink/filter to be added
         * @param type One element or some bitwise or combination of elements of IFrameSource::CB_TYPE
         * @return Current position in callback list
         */
        Q_INVOKABLE qint64 registerCallback(IFrameSink *frameSink) Q_DECL_OVERRIDE;

        /*!
         * \brief Removes frame sink/filter from registry
         * @param frameSink Frame sink/filter to be removed
         * @return Last position in callback list, -1 when not found
         */
        Q_INVOKABLE qint64 unregisterCallback(IFrameSink *frameSink) Q_DECL_OVERRIDE;

    public slots:
        /*!
         * \brief Initialize decoder. Opens input device, creates input contexts.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int init() Q_DECL_OVERRIDE;

        /*!
         * \brief Clean up decoder. Closes input device, destroys input contexts.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int deinit() Q_DECL_OVERRIDE;

        /*!
         * \brief Starts decoder. Sets running flag and starts decoding thread.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int start() Q_DECL_OVERRIDE;

        /*!
         * \brief Stops decoder. Resets running flag and interrupts decoding thread.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int stop() Q_DECL_OVERRIDE;

        /*!
         * \brief Sets paused flag. When set to true, decoder thread will suspend after processing current frame
         * @param pause New paused flag state
         */
        Q_INVOKABLE void pause(bool pause) Q_DECL_OVERRIDE;


        Q_INVOKABLE void
        init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
             AVCodecParameters *aParams, AVCodecParameters *sParams) Q_DECL_OVERRIDE;

        Q_INVOKABLE void deinit(IPacketSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE void start(IPacketSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE void stop(IPacketSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE void onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) Q_DECL_OVERRIDE;

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
         * Protected constructor for use in derived classes to initialize private struct
         * @param p Private struct
         */
        [[maybe_unused]] explicit DecoderMMAL(DecoderMMALPrivate &p);

        /*!
         * \private
         */
        void run() Q_DECL_OVERRIDE;

        /*!
         * \private
         */
        DecoderMMALPrivate *d_ptr;
    };

}

#endif //TRANSCODE_DECODERMMAL_H
