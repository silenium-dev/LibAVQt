//
// Created by silas on 3/25/21.
//

#include <QObject>
//#include "input/IPacketSource.h"

extern "C" {
#include <libavutil/rational.h>
}

#ifndef LIBAVQT_IPACKETSINK_H
#define LIBAVQT_IPACKETSINK_H

struct AVPacket;
struct AVCodecParameters;

namespace AVQt {
    class IPacketSource;

    class IPacketSink {
    public:
        virtual ~IPacketSink() = default;

        virtual bool isPaused() = 0;

    public slots:
        /*!
         * \brief Initialize packet sink
         *
         * Is called by packet source for every input stream (video, audio, subtitle), when the source is initialized or when packet sink is registered on an already started source.
         * When the packet sink is registered at multiple packet sources, it must be able to handle multiple calls for every stream type
         * @param cbType Information's stream type as IPacketSource::CB_TYPE
         * @param streamDuration Stream duration in ms
         * @param streamTimebase Stream timebase as AVRational
         */
        Q_INVOKABLE virtual void
        init(IPacketSource *source, AVRational framerate, int64_t duration, AVCodecParameters *vParams, AVCodecParameters *aParams,
             AVCodecParameters *sParams) = 0;

        Q_INVOKABLE virtual void deinit(IPacketSource *source) = 0;

        Q_INVOKABLE virtual void start(IPacketSource *source) = 0;

        Q_INVOKABLE virtual void stop(IPacketSource *source) = 0;

        Q_INVOKABLE virtual void pause(bool paused) = 0;

        /*!
         * \brief Callback method, is called for every registered packet type.
         *
         * There is only one method for all callback types, so it has distinguish between packet types
         * @param source Packet source that generated the packet
         * @param packet AVPacket, contains all necessary information
         * @param packetType Packet type as IPacketSource::CB_TYPE
         */
        Q_INVOKABLE virtual void onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) = 0;

    signals:

        virtual void started() = 0;

        virtual void stopped() = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IPacketSink, "IPacketSink")

#endif //LIBAVQT_IPACKETSINK_H
