//
// Created by silas on 3/28/21.
//

#include "output/IPacketSink.h"
#include "input/IAudioSource.h"

#include <QThread>

#ifndef LIBAVQT_AUDIODECODER_H
#define LIBAVQT_AUDIODECODER_H


namespace AVQt {
    class AudioDecoderPrivate;

    class AudioDecoder : public QThread, public IPacketSink, public IAudioSource {
    Q_OBJECT
        Q_INTERFACES(AVQt::IPacketSink)
        Q_INTERFACES(AVQt::IAudioSource)

        Q_DECLARE_PRIVATE(AVQt::AudioDecoder)

    public:
        explicit AudioDecoder(QObject *parent = nullptr);

        ~AudioDecoder() Q_DECL_OVERRIDE;

        bool isPaused() Q_DECL_OVERRIDE;

        int registerCallback(IAudioSink *callback) Q_DECL_OVERRIDE;

        int unregisterCallback(IAudioSink *callback) Q_DECL_OVERRIDE;

        void run() Q_DECL_OVERRIDE;

    public slots:

        Q_INVOKABLE int init() Q_DECL_OVERRIDE;

        Q_INVOKABLE int deinit() Q_DECL_OVERRIDE;

        Q_INVOKABLE int start() Q_DECL_OVERRIDE;

        Q_INVOKABLE int stop() Q_DECL_OVERRIDE;

        Q_INVOKABLE void
        init(IPacketSource *source, AVRational framerate, int64_t duration, AVCodecParameters *vParams, AVCodecParameters *aParams,
             AVCodecParameters *sParams) Q_DECL_OVERRIDE;

        Q_INVOKABLE void deinit(IPacketSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE void start(IPacketSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE void stop(IPacketSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE void pause(bool paused) Q_DECL_OVERRIDE;

        Q_INVOKABLE void onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) Q_DECL_OVERRIDE;

    signals:

        void started() Q_DECL_OVERRIDE;

        void stopped() Q_DECL_OVERRIDE;

        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        [[maybe_unused]] explicit AudioDecoder(AudioDecoderPrivate &p);

        AudioDecoderPrivate *d_ptr{};
    };
}

#endif //LIBAVQT_AUDIODECODER_H
