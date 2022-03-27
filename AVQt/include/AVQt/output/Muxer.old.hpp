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

#include "IPacketSink.hpp"

#include <QIODevice>
#include <QThread>

#ifndef LIBAVQT_MUXER_H
#define LIBAVQT_MUXER_H

namespace AVQt {
    class MuxerPrivate;

    class Muxer : public QThread, public IPacketSink {
        Q_OBJECT
        Q_INTERFACES(AVQt::IPacketSink)

        Q_DECLARE_PRIVATE(AVQt::Muxer)

    public:
        enum class FORMAT {
            MP4,
            MOV,
            MKV,
            WEBM,
            MPEGTS,
            INVALID
        };

        explicit Muxer(QIODevice *outputDevice, FORMAT format, QObject *parent = nullptr);

        Muxer(Muxer &) = delete;

        Muxer(Muxer &&other) noexcept;

        bool isPaused() Q_DECL_OVERRIDE;

        void init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                  AVCodecParameters *aParams, AVCodecParameters *sParams) Q_DECL_OVERRIDE;

        void deinit(IPacketSource *source) Q_DECL_OVERRIDE;

        void start(IPacketSource *source) Q_DECL_OVERRIDE;

        void stop(IPacketSource *source) Q_DECL_OVERRIDE;

        void pause(bool p) Q_DECL_OVERRIDE;

        void onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) Q_DECL_OVERRIDE;

        void operator=(const Muxer &) = delete;

    protected:
        void run() Q_DECL_OVERRIDE;

    signals:

        void started() Q_DECL_OVERRIDE;

        void stopped() Q_DECL_OVERRIDE;

        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        [[maybe_unused]] [[maybe_unused]] explicit Muxer(MuxerPrivate &p);

        MuxerPrivate *d_ptr;
    };
}// namespace AVQt

#endif//LIBAVQT_MUXER_H
