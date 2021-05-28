//
// Created by silas on 5/13/21.
//

#include "output/IFrameSink.h"
#include "input/IPacketSource.h"

#ifndef LIBAVQT_IENCODER_H
#define LIBAVQT_IENCODER_H

namespace AVQt {
    class IEncoder : public IFrameSink, public IPacketSource {

        Q_INTERFACES(AVQt::IFrameSink)
        Q_INTERFACES(AVQt::IPacketSource)

    public:
        virtual ~IEncoder() = default;

        virtual bool isPaused() = 0;

    public slots:

        Q_INVOKABLE virtual int init(IFrameSource *source, AVRational framerate, int64_t duration) = 0;

        Q_INVOKABLE virtual int deinit(IFrameSource *source) = 0;

        Q_INVOKABLE virtual int start(IFrameSource *source) = 0;

        Q_INVOKABLE virtual int stop(IFrameSource *source) = 0;

        Q_INVOKABLE virtual void pause(IFrameSource *source, bool pause) = 0;

        Q_INVOKABLE virtual void onFrame(IFrameSource *source, AVFrame *frame, int64_t frameDuration) = 0;


        // IPacketSource

        Q_INVOKABLE virtual int init() = 0;

        Q_INVOKABLE virtual int deinit() = 0;

        Q_INVOKABLE virtual int start() = 0;

        Q_INVOKABLE virtual int stop() = 0;

        Q_INVOKABLE virtual void pause(bool pause) = 0;

        Q_INVOKABLE virtual qint64 registerCallback(IPacketSink *packetSink, int8_t type) = 0;

        Q_INVOKABLE virtual qint64 unregisterCallback(IPacketSink *packetSink) = 0;

    signals:

        virtual void started() = 0;

        virtual void stopped() = 0;

        virtual void paused(bool pause) = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IEncoder, "AVQt::IEncoder")

#endif //LIBAVQT_IENCODER_H