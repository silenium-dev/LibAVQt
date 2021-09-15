//
// Created by silas on 01.09.21.
//

#ifndef LIBAVQT_OPENALAUDIOOUTPUT_H
#define LIBAVQT_OPENALAUDIOOUTPUT_H

#include <QThread>

#include "output/IAudioSink.h"
#include "output/IFrameSink.h"

namespace AVQt {
    class OpenALAudioOutputPrivate;

    class OpenALAudioOutput : public QThread, public IAudioSink {
        Q_OBJECT
        Q_INTERFACES(AVQt::IAudioSink)

        Q_DECLARE_PRIVATE(AVQt::OpenALAudioOutput)

    public:
        explicit OpenALAudioOutput(QObject *parent = nullptr);

        OpenALAudioOutput(OpenALAudioOutput &&other) noexcept;

        bool isPaused() override;

    public slots:

        int init(AVQt::IAudioSource *source, int64_t duration, int sampleRate, AVSampleFormat sampleFormat, uint64_t channelLayout) override;

        int deinit(AVQt::IAudioSource *source) override;

        int start(AVQt::IAudioSource *source) override;

        int stop(AVQt::IAudioSource *source) override;

        void pause(AVQt::IAudioSource *source, bool pause) override;

        void onAudioFrame(AVQt::IAudioSource *source, AVFrame *frame, uint32_t duration) override;

        void enqueueAudioForFrame(qint64 pts, qint64 duration);

        void syncToOutput(AVQt::IFrameSink *output);

    signals:

        void started() Q_DECL_OVERRIDE;

        void stopped() Q_DECL_OVERRIDE;

        void paused(bool paused) Q_DECL_OVERRIDE;

    protected:
        [[maybe_unused]] explicit OpenALAudioOutput(OpenALAudioOutputPrivate &p);

        void run() Q_DECL_OVERRIDE;

        OpenALAudioOutputPrivate *d_ptr;
    };
}


#endif //LIBAVQT_OPENALAUDIOOUTPUT_H