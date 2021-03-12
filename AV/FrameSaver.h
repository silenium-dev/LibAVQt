//
// Created by silas on 3/3/21.
//

#include "IFrameSink.h"

#include <QtGui>

#ifndef TRANSCODE_FRAMESAVER_H
#define TRANSCODE_FRAMESAVER_H

namespace AV {
    class FrameSaver : public QObject, public IFrameSink {
    Q_OBJECT
        Q_INTERFACES(AV::IFrameSink)
    public:
        explicit FrameSaver(QObject *parent = nullptr);

    public slots:
        Q_INVOKABLE int init() override;

        Q_INVOKABLE int deinit() override;

        Q_INVOKABLE int start() override;

        Q_INVOKABLE int stop() override;

        Q_INVOKABLE int pause(bool paused) override;

        Q_INVOKABLE void onFrame(QImage frame, AVRational timebase, AVRational framerate) override;

        Q_INVOKABLE void
        onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) override;

    private:
        std::atomic_uint64_t frame_number = 0;
        std::atomic_bool isPaused = false;
    };
}


#endif //TRANSCODE_FRAMESAVER_H
