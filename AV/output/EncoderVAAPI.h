//
// Created by silas on 3/3/21.
//

#include "IFrameSink.h"

#include <QThread>

#ifndef TRANSCODE_ENCODERVAAPI_H
#define TRANSCODE_ENCODERVAAPI_H

namespace AV {
    class EncoderVAAPIPrivate;

    class EncoderVAAPI : public QThread, public IFrameSink {
    Q_OBJECT
        Q_INTERFACES(AV::IFrameSink)

        Q_DECLARE_PRIVATE(AV::EncoderVAAPI)

    public:
        enum class VIDEO_CODEC {
            H264, HEVC, VP9
        };

        enum class AUDIO_CODEC {
            AAC, OGG, OPUS
        };

        explicit EncoderVAAPI(QIODevice *outputDevice, VIDEO_CODEC videoCodec, AUDIO_CODEC audioCodec, int width, int height,
                              AVRational framerate, int bitrate,
                              QObject *parent = nullptr);

        ~EncoderVAAPI() override;

        void run() override;

    public slots:
        Q_INVOKABLE int init() override;

        Q_INVOKABLE int deinit() override;

        Q_INVOKABLE int start() override;

        Q_INVOKABLE int stop() override;

        Q_INVOKABLE int pause(bool pause) override;

        Q_INVOKABLE void onFrame(QImage frame, AVRational timebase, AVRational framerate) override;

        Q_INVOKABLE void onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) override;

    signals:

        void started() override;

        void stopped() override;

    protected:
        explicit EncoderVAAPI(EncoderVAAPIPrivate &p);

        EncoderVAAPIPrivate *d_ptr;
    };
}

#endif //TRANSCODE_ENCODERVAAPI_H
