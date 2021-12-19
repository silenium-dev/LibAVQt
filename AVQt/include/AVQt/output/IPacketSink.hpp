// Copyright (c) 2021.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef LIBAVQT_IPACKETSINK_H
#define LIBAVQT_IPACKETSINK_H

#include <QObject>

extern "C" {
#include <libavutil/rational.h>
}

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
        Q_INVOKABLE virtual void init(AVQt::IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration,
                                      AVCodecParameters *vParams, AVCodecParameters *aParams, AVCodecParameters *sParams) = 0;

        Q_INVOKABLE virtual void deinit(AVQt::IPacketSource *source) = 0;

        Q_INVOKABLE virtual void start(AVQt::IPacketSource *source) = 0;

        Q_INVOKABLE virtual void stop(AVQt::IPacketSource *source) = 0;

        Q_INVOKABLE virtual void pause(bool p) = 0;

        /*!
         * \brief Callback method, is called for every registered packet type.
         *
         * There is only one method for all callback types, so it has distinguish between packet types
         * @param source Packet source that generated the packet
         * @param packet AVPacket, contains all necessary information
         * @param packetType Packet type as IPacketSource::CB_TYPE
         */
        Q_INVOKABLE virtual void onPacket(AVQt::IPacketSource *source, AVPacket *packet, int8_t packetType) = 0;

    signals:

        virtual void started() = 0;

        virtual void stopped() = 0;

        virtual void paused(bool pause) = 0;
    };
}// namespace AVQt

Q_DECLARE_INTERFACE(AVQt::IPacketSink, "IPacketSink")

#endif//LIBAVQT_IPACKETSINK_H
