//
// Created by silas on 5/24/21.
//

#include "IPacketSink.h"

#include <QThread>

#ifndef LIBAVQT_MUXER_H
#define LIBAVQT_MUXER_H

namespace AVQt {
    class MuxerPrivate;

    class Muxer : public QThread, public IPacketSink {
    Q_OBJECT
        Q_INTERFACES(AVQt::IPacketSink)

        Q_DECLARE_PRIVATE(AVQt::Muxer)

    protected:
        void run() Q_DECL_OVERRIDE;

    public:
        explicit Muxer(QIODevice *outputDevice, QObject *parent = nullptr);

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

    signals:

        void started() Q_DECL_OVERRIDE;

        void stopped() Q_DECL_OVERRIDE;

        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        [[maybe_unused]] explicit Muxer(MuxerPrivate &p);

        MuxerPrivate *d_ptr;
    };
}

#endif //LIBAVQT_MUXER_H