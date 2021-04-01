//
// Created by silas on 3/3/21.
//

#include "output/IFrameSink.h"

#include <QThread>

#ifndef TRANSCODE_ENCODERVAAPI_H
#define TRANSCODE_ENCODERVAAPI_H

namespace AVQt {
    class EncoderVAAPIPrivate;

    /*!
     * \class EncoderVAAPI
     * \brief VAAPI accelerated video encoder
     *
     * Sink for encoding every type of frame into mpegts file with provided codecs. Same instance cannot be used as callback of both types.
     * ***Requires constant frame input or else the output will be scrambled. This is preliminary and will be subject to later changes.***
     */
    class EncoderVAAPI : public QThread, public IFrameSink {
    Q_OBJECT
        Q_INTERFACES(AVQt::IFrameSink)

        Q_DECLARE_PRIVATE(AVQt::EncoderVAAPI)

    public:
        /*!
         * \brief Enum for video codec
         */
        enum class VIDEO_CODEC {
            /*!
             * \brief Selects video codec ``h264_vaapi`` for encoding video
             */
            H264,

            /*!
             * \brief Selects video codec ``hevc_vaapi`` for encoding video
             */
            HEVC,
            /*!
             * \brief Selects video codec ``vp9_vaapi`` for encoding video
             *
             * ***Important:*** On intel CPUs this only works for Braswell - Kabylake with the i965 VAAPI driver,
             * as the iHD driver does not contain support for VP9 encoding
             */
            VP9
        };

        /*!
         * \brief Currently unused
         */
        enum class AUDIO_CODEC {
            AAC, OGG, OPUS
        };

        /*!
         * \brief Standard public contructor
         *
         * Creates encoder with output QIODevice \c outputDevice, video encoder \c videoCodec, audio encoder \c audioCodec
         * and the given video parameters. If the video parameters are different to the input frames, the frames will be scaled or dropped.
         * @param outputDevice QIODevice, either already opened for write access or closed.
         * If closed, it will be opened for write access in init().
         * @param videoCodec Video encoder from \c VIDEO_CODEC
         * @param audioCodec Audio encoder from \c AUDIO_CODEC (currently unused)
         * @param width Target video width (currently unused)
         * @param height Target video height (currently unused)
         * @param framerate Target video framerate (currently unused)
         * @param bitrate Target video bitrate
         * @param parent Pointer to parent QObject for Qt's meta object system
         */
        explicit EncoderVAAPI(QIODevice *outputDevice, VIDEO_CODEC videoCodec, AUDIO_CODEC audioCodec, int width, int height,
                              AVRational framerate, int bitrate,
                              QObject *parent = nullptr);

        /*!
         * \private
         */
        ~EncoderVAAPI() Q_DECL_OVERRIDE;

        /*!
         * \private
         */
        void run() Q_DECL_OVERRIDE;

    public slots:
        /*!
         * \brief Initialize encoder
         *
         * Opens output device, if necessary and creates output context.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int init(int64_t duration) Q_DECL_OVERRIDE;

        /*!
         * \brief Clean up encoder, encoder will be stopped before, if running
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int deinit() Q_DECL_OVERRIDE;

        /*!
         * \brief Start encoder thread
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int start() Q_DECL_OVERRIDE;

        /*!
         * \brief Interrupt encoder thread and waits for it to finish.
         * @return Status code (0 = Success, LibAV error codes, use av_make_error_string() to get an error message)
         */
        Q_INVOKABLE int stop() Q_DECL_OVERRIDE;

        /*!
         * \brief Sets paused flag to value of \c pause
         *
         * If the paused flag is set, the encoder thread will suspend. The frame queue will be emptied every 4 ms if paused.
         * @param pause New paused state
         */
        Q_INVOKABLE void pause(bool pause) Q_DECL_OVERRIDE;

        /*!
         * \brief Returns current paused state
         * @return Current paused state
         */
        bool isPaused() Q_DECL_OVERRIDE;

        /*!
         * \brief Add frame to encode queue, blocks if queue is full
         * @param frame Frame to add
         * @param entireDuration decode timebase (***Unstable API***)
         * @param framerate decode framerate (***Unstable API***)
         */
        Q_INVOKABLE void onFrame(QImage frame, int64_t duration) Q_DECL_OVERRIDE;

        /*!
         * \brief Add frame to encode queue, blocks if queue is full
         * @param frame Frame to add
         * @param timebase decode timebase (***Unstable API***)
         * @param framerate decode framerate (***Unstable API***)
         */
        Q_INVOKABLE void onFrame(AVFrame *frame, AVRational timebase, AVRational framerate, int64_t duration) Q_DECL_OVERRIDE;

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
         * Emitted when paused state changes
         * @param pause Current paused state
         */
        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        /*!
         * Protected constructor for use in derived classes to initialize private struct
         * @param p Private struct
         */
        explicit EncoderVAAPI(EncoderVAAPIPrivate &p);

        /*!
         * \private
         */
        EncoderVAAPIPrivate *d_ptr;
    };
}

#endif //TRANSCODE_ENCODERVAAPI_H
