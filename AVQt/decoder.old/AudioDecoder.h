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

#include "output/IPacketSink.h"
#include "AVQt/input/IAudioSource.h"

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

        AudioDecoder(AudioDecoder &&other) noexcept;

        AudioDecoder(const AudioDecoder &) = delete;

        void operator=(const AudioDecoder &) = delete;

        ~AudioDecoder() Q_DECL_OVERRIDE;

        bool isPaused() Q_DECL_OVERRIDE;

        qint64 registerCallback(IAudioSink *callback) Q_DECL_OVERRIDE;

        qint64 unregisterCallback(IAudioSink *callback) Q_DECL_OVERRIDE;

        void run() Q_DECL_OVERRIDE;

    public slots:

        Q_INVOKABLE int init() Q_DECL_OVERRIDE;

        Q_INVOKABLE int deinit() Q_DECL_OVERRIDE;

        Q_INVOKABLE int start() Q_DECL_OVERRIDE;

        Q_INVOKABLE int stop() Q_DECL_OVERRIDE;

        Q_INVOKABLE void
        init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
             AVCodecParameters *aParams, AVCodecParameters *sParams) Q_DECL_OVERRIDE;

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
