//
// Created by silas on 4/18/21.
//

#include "output/IFrameSink.h"
#include "input/IPacketSource.h"

#include <QThread>

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

#ifndef LIBAVQT_ENCODERVAAPI_H
#define LIBAVQT_ENCODERVAAPI_H

namespace AVQt {
    class EncoderVAAPIPrivate;

    class EncoderVAAPI : public QThread, public IFrameSink, public IPacketSource {
    Q_OBJECT

        Q_DECLARE_PRIVATE(AVQt::EncoderVAAPI)

        Q_INTERFACES(AVQt::IFrameSink)
        Q_INTERFACES(AVQt::IPacketSource)

    public:
        explicit EncoderVAAPI(QString encoder, QObject *parent = nullptr);

        ~EncoderVAAPI() override;

        bool isPaused() override;

        qsizetype registerCallback(IPacketSink *packetSink, uint8_t type) override;

        qsizetype unregisterCallback(IPacketSink *packetSink) override;

        void run() override;

    public slots:

        Q_INVOKABLE int init() override;

        Q_INVOKABLE int deinit() override;

        Q_INVOKABLE int start() override;

        Q_INVOKABLE int stop() override;

        Q_INVOKABLE void pause(bool pause) override;

        Q_INVOKABLE int init(IFrameSource *source, AVRational framerate, int64_t duration) override;

        Q_INVOKABLE int deinit(IFrameSource *source) override;

        Q_INVOKABLE int start(IFrameSource *source) override;

        Q_INVOKABLE int stop(IFrameSource *source) override;

        Q_INVOKABLE void pause(IFrameSource *source, bool paused) override;

        Q_INVOKABLE void onFrame(IFrameSource *source, AVFrame *frame, int64_t frameDuration) override;

    signals:

        void started() override;

        void stopped() override;

        void paused(bool pause) override;

    protected:
        explicit EncoderVAAPI(EncoderVAAPIPrivate &p);

        EncoderVAAPIPrivate *d_ptr;

    };
}

#endif //LIBAVQT_ENCODERVAAPI_H