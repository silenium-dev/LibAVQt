//
// Created by silas on 3/28/21.
//

#include "IAudioSink.h"

#include <QThread>

#ifndef LIBAVQT_OPENALAUDIOOUTPUT_H
#define LIBAVQT_OPENALAUDIOOUTPUT_H


namespace AVQt {
    class OpenALAudioOutputPrivate;

    class OpenGLRenderer;

    class OpenALAudioOutput : public QThread, public IAudioSink {
    Q_OBJECT
        Q_INTERFACES(AVQt::IAudioSink)

        Q_DECLARE_PRIVATE(AVQt::OpenALAudioOutput)

    public:
        explicit OpenALAudioOutput(QObject *parent = nullptr);

        Q_INVOKABLE bool isPaused() Q_DECL_OVERRIDE;

        void run() Q_DECL_OVERRIDE;

        void syncToOutput(OpenGLRenderer *renderer);

    public slots:

        Q_INVOKABLE int init(IAudioSource *source, int64_t duration) Q_DECL_OVERRIDE;

        Q_INVOKABLE int deinit(IAudioSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE int start(IAudioSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE int stop(IAudioSource *source) Q_DECL_OVERRIDE;

        Q_INVOKABLE void pause(IAudioSource *source, bool pause) Q_DECL_OVERRIDE;

        Q_INVOKABLE void onAudioFrame(IAudioSource *source, AVFrame *frame, uint32_t duration) Q_DECL_OVERRIDE;

    signals:

        void started() Q_DECL_OVERRIDE;

        void stopped() Q_DECL_OVERRIDE;

        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        [[maybe_unused]] explicit OpenALAudioOutput(OpenALAudioOutputPrivate &p);

        OpenALAudioOutputPrivate *d_ptr;

    private slots:

        void clockIntervalChanged(int64_t interval);
    };
}


#endif //LIBAVQT_OPENALAUDIOOUTPUT_H
