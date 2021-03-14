//
// Created by silas on 3/3/21.
//

#include "private/FrameSaver_p.h"
#include "FrameSaver.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}

#include <QtCore>
#include <QImage>

namespace AV {
    FrameSaver::FrameSaver(QObject *parent) : QObject(parent), d_ptr(new FrameSaverPrivate(this)) {

    }

    FrameSaver::FrameSaver(FrameSaverPrivate &p) : d_ptr(&p) {

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
        Q_D(AV::FrameSaver);
        Q_UNUSED(timebase)
        Q_UNUSED(framerate)
        if (!d->m_isPaused.load() && d->m_frameNumber.load() % 30 == 0) {
//        if (!d->m_isPaused.load()) {
            qDebug() << "Saving frame" << d->m_frameNumber.load();
            char filename[32];
            snprintf(filename, 32, "frame-%lu.png", d->m_frameNumber++);
            frame.save(filename);
        } else {
            ++d->m_frameNumber;
        }
    }

    int FrameSaver::pause(bool paused) {
        Q_D(AV::FrameSaver);
        d->m_isPaused.store(paused);
        return 0;
    }

    void
    FrameSaver::onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) {
        av_frame_unref(frame);
    }

}