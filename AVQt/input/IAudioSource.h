//
// Created by silas on 3/17/21.
//

#include <QObject>

#ifndef LIBAVQT_IAUDIOSOURCE_H
#define LIBAVQT_IAUDIOSOURCE_H

struct AVFrame;

namespace AVQt {
    class IAudioSink;

    class IAudioSource {
    public:
        virtual ~IAudioSource() = default;

        virtual bool isPaused() = 0;

        Q_INVOKABLE virtual qint64 registerCallback(AVQt::IAudioSink *callback) = 0;

        Q_INVOKABLE virtual qint64 unregisterCallback(AVQt::IAudioSink *callback) = 0;

    public slots:
        Q_INVOKABLE virtual int init() = 0;

        Q_INVOKABLE virtual int deinit() = 0;

        Q_INVOKABLE virtual int start() = 0;

        Q_INVOKABLE virtual int stop() = 0;

        Q_INVOKABLE virtual void pause(bool isPaused) = 0;

    signals:

        virtual void started() = 0;

        virtual void stopped() = 0;

        virtual void paused(bool pause) = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IAudioSource, "AVQt::IAudioSink")

#endif //LIBAVQT_IAUDIOSOURCE_H