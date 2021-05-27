//
// Created by silas on 4/18/21.
//

#include "IEncoder.h"

#include <QThread>

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

#ifndef LIBAVQT_ENCODERVAAPI_H
#define LIBAVQT_ENCODERVAAPI_H

namespace AVQt {
    class EncoderVAAPIPrivate;

    class EncoderVAAPI : public QThread, public IEncoder {
    Q_OBJECT

        Q_DECLARE_PRIVATE(AVQt::EncoderVAAPI)

        Q_INTERFACES(AVQt::IEncoder)

    public:
        explicit EncoderVAAPI(QString encoder, QObject *parent = nullptr);

        EncoderVAAPI(EncoderVAAPI &&other) noexcept;

        explicit EncoderVAAPI(EncoderVAAPI &other) = delete;

        ~EncoderVAAPI() Q_DECL_OVERRIDE;

        bool isPaused() Q_DECL_OVERRIDE;

        qsizetype registerCallback(IPacketSink *packetSink, int8_t type) Q_DECL_OVERRIDE;

        qsizetype unregisterCallback(IPacketSink *packetSink) Q_DECL_OVERRIDE;

        void run() Q_DECL_OVERRIDE;

        EncoderVAAPI &operator=(const EncoderVAAPI &other) = delete;

        EncoderVAAPI &operator=(EncoderVAAPI &&other) noexcept;

    public slots:

        Q_INVOKABLE int init() Q_DECL_OVERRIDE;

        Q_INVOKABLE int deinit() Q_DECL_OVERRIDE;

        Q_INVOKABLE int start() Q_DECL_OVERRIDE;

        Q_INVOKABLE int stop() Q_DECL_OVERRIDE;

        Q_INVOKABLE void pause(bool pause) Q_DECL_OVERRIDE;

        Q_INVOKABLE int init(IFrameSource *source, AVRational framerate, int64_t duration) Q_DECL_OVERRIDE;

        Q_INVOKABLE int deinit(IFrameSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE int start(IFrameSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE int stop(IFrameSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE void pause(IFrameSource *source, bool paused) Q_DECL_OVERRIDE;

        Q_INVOKABLE void onFrame(IFrameSource *source, AVFrame *frame, int64_t frameDuration) Q_DECL_OVERRIDE;

    signals:

        void started() Q_DECL_OVERRIDE;

        void stopped() Q_DECL_OVERRIDE;

        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        [[maybe_unused]] explicit EncoderVAAPI(EncoderVAAPIPrivate &p);

        EncoderVAAPIPrivate *d_ptr;

    };
}

#endif //LIBAVQT_ENCODERVAAPI_H