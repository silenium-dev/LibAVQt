//
// Created by silas on 5/1/21.
//

#include "input/IFrameSource.h"
#include "output/IPacketSink.h"

#ifndef LIBAVQT_DECODER_H
#define LIBAVQT_DECODER_H

namespace AVQt {
    class IDecoder : public IFrameSource, public IPacketSink {


        // IPacketSink interface
    public:
        ~IDecoder() Q_DECL_OVERRIDE {};

        virtual bool isPaused() Q_DECL_OVERRIDE = 0;

        virtual void init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                          AVCodecParameters *aParams, AVCodecParameters *sParams) Q_DECL_OVERRIDE = 0;

        virtual void deinit(IPacketSource *source) Q_DECL_OVERRIDE = 0;

        virtual void start(IPacketSource *source) Q_DECL_OVERRIDE = 0;

        virtual void stop(IPacketSource *source) Q_DECL_OVERRIDE = 0;

        virtual void pause(bool paused) Q_DECL_OVERRIDE = 0;

        virtual void onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) Q_DECL_OVERRIDE = 0;

    signals:

        virtual void started() Q_DECL_OVERRIDE = 0;

        virtual void stopped() Q_DECL_OVERRIDE = 0;

        // IFrameSource interface
    public:
        virtual qsizetype registerCallback(IFrameSink *frameSink) Q_DECL_OVERRIDE = 0;

        virtual qsizetype unregisterCallback(IFrameSink *frameSink) Q_DECL_OVERRIDE = 0;

        virtual int init() Q_DECL_OVERRIDE = 0;

        virtual int deinit() Q_DECL_OVERRIDE = 0;

        virtual int start() Q_DECL_OVERRIDE = 0;

        virtual int stop() Q_DECL_OVERRIDE = 0;

    signals:

        virtual void paused(bool pause) Q_DECL_OVERRIDE = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IDecoder, "AVQt::IDecoder")

#endif //LIBAVQT_DECODER_H