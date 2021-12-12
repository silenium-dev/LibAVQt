#include "include/AVQt/output/IFrameSink.hpp"
#include "include/AVQt/input/IPacketSource.hpp"

#ifndef LIBAVQT_IENCODER_H
#define LIBAVQT_IENCODER_H

namespace AVQt {
    class IEncoder : public IFrameSink, public IPacketSource {

        Q_INTERFACES(AVQt::IFrameSink)
        Q_INTERFACES(AVQt::IPacketSource)

    public:
        enum class CODEC {
            H264, HEVC, VP9, VP8, MPEG2, AV1
        };

        ~IEncoder() Q_DECL_OVERRIDE = default;

        bool isPaused() Q_DECL_OVERRIDE = 0;

    public slots:

        Q_INVOKABLE int init(IFrameSource *source, AVRational framerate, int64_t duration) Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE int deinit(IFrameSource *source) Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE int start(IFrameSource *source) Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE int stop(IFrameSource *source) Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE void pause(IFrameSource *source, bool pause) Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE void onFrame(IFrameSource *source, AVFrame *frame, int64_t frameDuration, AVBufferRef *pDeviceCtx) Q_DECL_OVERRIDE = 0;


        // IPacketSource

        Q_INVOKABLE int init() Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE int deinit() Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE int start() Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE int stop() Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE void pause(bool pause) Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE qint64 registerCallback(IPacketSink *packetSink, int8_t type) Q_DECL_OVERRIDE = 0;

        Q_INVOKABLE qint64 unregisterCallback(IPacketSink *packetSink) Q_DECL_OVERRIDE = 0;

    signals:

        void started() Q_DECL_OVERRIDE = 0;

        void stopped() Q_DECL_OVERRIDE = 0;

        void paused(bool pause) Q_DECL_OVERRIDE = 0;

        void frameProcessingStarted(qint64 pts, qint64 duration) Q_DECL_OVERRIDE = 0;

        void frameProcessingFinished(qint64 pts, qint64 duration) Q_DECL_OVERRIDE = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IEncoder, "AVQt::IEncoder")

#endif //LIBAVQT_IENCODER_H
