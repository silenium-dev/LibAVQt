#include "private/FrameFileSaver_p.h"
#include "FrameFileSaver.h"

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
#include <utility>

namespace AVQt {
    FrameFileSaver::FrameFileSaver(quint64 interval, QString filePrefix, QObject *parent) : QObject(parent),
                                                                                            d_ptr(new FrameFileSaverPrivate(this)) {
        d_ptr->m_frameInterval = interval;
        d_ptr->m_filePrefix = std::move(filePrefix);
    }

    FrameFileSaver::FrameFileSaver(FrameFileSaverPrivate &p) : d_ptr(&p) {

    }

    int FrameFileSaver::init(int64_t duration) {
        return 0;
    }

    int FrameFileSaver::deinit() {
        return 0;
    }

    int FrameFileSaver::start() {
        started();
        return 0;
    }

    int FrameFileSaver::stop() {
        stopped();
        return 0;
    }

    void FrameFileSaver::onFrame(QImage frame, int64_t duration) {
        Q_D(AVQt::FrameFileSaver);
        if (!d->m_isPaused.load() && d->m_frameNumber.load() % d->m_frameInterval == 0) {
//        if (!d->m_isPaused.load()) {
//            qDebug() << "Saving frame" << d->m_frameNumber.load();
            char filename[32];
            snprintf(filename, 32, "frame-%lu.bmp", d->m_frameNumber++);
            frame.save(d->m_filePrefix + "-" + filename);
        } else {
            ++d->m_frameNumber;
        }
    }

    void FrameFileSaver::onFrame(AVFrame *frame, AVRational timebase, AVRational framerate, int64_t duration) {
        av_frame_unref(frame);
    }

    void FrameFileSaver::pause(bool pause) {
        Q_D(AVQt::FrameFileSaver);
        if (d->m_isPaused.load() != pause) {
            d->m_isPaused.store(pause);
            paused(pause);
        }
    }

    bool FrameFileSaver::isPaused() {
        Q_D(AVQt::FrameFileSaver);
        return d->m_isPaused.load();
    }

    FrameFileSaver::~FrameFileSaver() {
        delete d_ptr;
        d_ptr = nullptr;
    }

}
