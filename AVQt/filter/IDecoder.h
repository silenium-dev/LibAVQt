//
// Created by silas on 5/1/21.
//

#include "input/IFrameSource.h"
#include "output/IPacketSink.h"

#ifndef LIBAVQT_DECODER_H
#define LIBAVQT_DECODER_H

namespace AVQt {
    class IDecoder: public IFrameSource, public IPacketSink {


        // IPacketSink interface
    public:
        ~IDecoder() override {};
        virtual bool isPaused() override = 0;
        virtual void init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams, AVCodecParameters *aParams, AVCodecParameters *sParams) override = 0;
        virtual void deinit(IPacketSource *source) override = 0;
        virtual void start(IPacketSource *source) override = 0;
        virtual void stop(IPacketSource *source) override = 0;
        virtual void pause(bool paused) override = 0;
        virtual void onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) override = 0;

    signals:
        virtual void started() override = 0;
        virtual void stopped() override = 0;

        // IFrameSource interface
    public:
        virtual int registerCallback(IFrameSink *frameSink) override = 0;
        virtual int unregisterCallback(IFrameSink *frameSink) override = 0;
        virtual int init() override = 0;
        virtual int deinit() override = 0;
        virtual int start() override = 0;
        virtual int stop() override = 0;

    signals:
        virtual void paused(bool pause) override = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IDecoder, "AVQt::IDecoder")

#endif //LIBAVQT_DECODER_H
