//
// Created by silas on 3/1/21.
//

#include <QImage>
#include <QObject>

#ifndef TRANSCODE_IFRAMESINK_H
#define TRANSCODE_IFRAMESINK_H

struct AVFrame;
struct AVRational;

namespace AV {
    class IFrameSink {
    public:
        virtual ~IFrameSink() = default;

    public slots:
        Q_INVOKABLE virtual int init() = 0;

        Q_INVOKABLE virtual int deinit() = 0;

        Q_INVOKABLE virtual int start() = 0;

        Q_INVOKABLE virtual int stop() = 0;

        Q_INVOKABLE virtual int pause(bool paused) = 0;

        Q_INVOKABLE virtual void onFrame(QImage frame, AVRational timebase, AVRational framerate) = 0;

        Q_INVOKABLE virtual void onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) = 0;

    signals:

        virtual void started() {};

        virtual void stopped() {};
    };
}

Q_DECLARE_INTERFACE(AV::IFrameSink, "IFrameSink")

#endif //TRANSCODE_IFRAMESINK_H
