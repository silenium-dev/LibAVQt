//
// Created by silas on 3/3/21.
//

#include "IFrameSink.h"

#ifndef TRANSCODE_FRAMESAVER_H
#define TRANSCODE_FRAMESAVER_H


namespace AV {
    class FrameSaverPrivate;

    class FrameSaver : public QObject, public IFrameSink {
    Q_OBJECT
        Q_INTERFACES(AV::IFrameSink)

        Q_DECLARE_PRIVATE(AV::FrameSaver)

    public:
        explicit FrameSaver(QObject *parent = nullptr);

    public slots:
        Q_INVOKABLE int init() override;

        Q_INVOKABLE int deinit() override;

        Q_INVOKABLE int start() override;

        Q_INVOKABLE int stop() override;

        Q_INVOKABLE int pause(bool paused) override;

        Q_INVOKABLE void onFrame(QImage frame, AVRational timebase, AVRational framerate) override;

        Q_INVOKABLE void onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) override;

    protected:
        explicit FrameSaver(FrameSaverPrivate &p);

        FrameSaverPrivate *d_ptr;
    };
}


#endif //TRANSCODE_FRAMESAVER_H
