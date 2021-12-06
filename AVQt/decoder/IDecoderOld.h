#include "input/IFrameSource.h"
#include "output/IPacketSink.h"

#ifndef LIBAVQT_DECODER_H
#define LIBAVQT_DECODER_H

namespace AVQt {
    class IDecoderOld : public IFrameSource, public IPacketSink {


        // IPacketSink interface
    public:
        ~IDecoderOld() Q_DECL_OVERRIDE = default;

        bool isPaused() Q_DECL_OVERRIDE = 0;

        void init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                  AVCodecParameters *aParams, AVCodecParameters *sParams) Q_DECL_OVERRIDE = 0;

        void deinit(IPacketSource *source) Q_DECL_OVERRIDE = 0;

        void start(IPacketSource *source) Q_DECL_OVERRIDE = 0;

        void stop(IPacketSource *source) Q_DECL_OVERRIDE = 0;

        void pause(bool paused) Q_DECL_OVERRIDE = 0;

        void onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) Q_DECL_OVERRIDE = 0;

    signals:

        void started() Q_DECL_OVERRIDE = 0;

        void stopped() Q_DECL_OVERRIDE = 0;

        // IFrameSource interface
    public:
        qint64 registerCallback(IFrameSink *frameSink) Q_DECL_OVERRIDE = 0;

        qint64 unregisterCallback(IFrameSink *frameSink) Q_DECL_OVERRIDE = 0;

        int init() Q_DECL_OVERRIDE = 0;

        int deinit() Q_DECL_OVERRIDE = 0;

        int start() Q_DECL_OVERRIDE = 0;

        int stop() Q_DECL_OVERRIDE = 0;

    signals:

        void paused(bool pause) Q_DECL_OVERRIDE = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IDecoderOld, "AVQt::IDecoderOld")

#endif //LIBAVQT_DECODER_H
