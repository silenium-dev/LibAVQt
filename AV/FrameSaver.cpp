//
// Created by silas on 3/3/21.
//

#include "FrameSaver.h"

namespace AV {
    FrameSaver::FrameSaver(QObject *parent) : QObject(parent) {

    }

    int FrameSaver::init() {
        return 0;
    }

    int FrameSaver::deinit() {
        return 0;
    }

    int FrameSaver::start() {
        return 0;
    }

    int FrameSaver::stop() {
        return 0;
    }

    void FrameSaver::onFrame(QImage frame, AVRational timebase, AVRational framerate) {
        Q_UNUSED(timebase)
        Q_UNUSED(framerate)
        if (!isPaused.load() && frame_number.load() % 30 == 0) {
//        if (!isPaused.load()) {
            qDebug() << "Saving frame" << frame_number.load();
            char filename[32];
            snprintf(filename, 32, "frame-%lu.png", frame_number++);
            frame.save(filename);
        } else {
            ++frame_number;
        }
    }

    int FrameSaver::pause(bool paused) {
        isPaused.store(paused);
        return 0;
    }

    void
    FrameSaver::onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) {
        av_frame_unref(frame);
    }

}