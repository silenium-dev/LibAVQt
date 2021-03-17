//
// Created by silas on 3/14/21.
//

#include "private/OpenGLWidgetRenderer_p.h"
#include "OpenGLWidgetRenderer.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <QtConcurrent>

#define NOW() std::chrono::high_resolution_clock::now();
#define TIME_US(t1, t2) std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()


namespace AVQt {
    OpenGLWidgetRenderer::OpenGLWidgetRenderer(QWidget *parent) : QOpenGLWidget(parent),
                                                                  d_ptr(new OpenGLWidgetRendererPrivate(this)) {
        Q_D(AVQt::OpenGLWidgetRenderer);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_NoSystemBackground);
    }

    OpenGLWidgetRenderer::OpenGLWidgetRenderer(OpenGLWidgetRendererPrivate &p) : d_ptr(&p) {
    }

    OpenGLWidgetRenderer::~OpenGLWidgetRenderer() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        QMutexLocker imageLock(&d->m_currentImageMutex);
        QMutexLocker queueLock(&d->m_imageInputQueueMutex);
    }

    int OpenGLWidgetRenderer::init() {
        return 0;
    }

    int OpenGLWidgetRenderer::deinit() {
        return 0;
    }

    int OpenGLWidgetRenderer::start() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->m_isPaused.store(false);
        d->m_isRunning.store(true);
        d->m_scalingFuture = QtConcurrent::run(&OpenGLWidgetRenderer::run, this);
        return 0;
    }

    int OpenGLWidgetRenderer::stop() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->m_isPaused.store(true);
        d->m_isRunning.store(false);
        {
            QMutexLocker lock(&d->m_imageInputQueueMutex);
            for (auto &e: d->m_imageInputQueue) {
                av_frame_unref(reinterpret_cast<AVFrame *>(*reinterpret_cast<uint64_t *>(e[1].data())));
            }
            d->m_imageInputQueue.clear();
        }
        {
            QMutexLocker lock(&d->m_imageRenderQueueMutex);
            d->m_imageRenderQueue.clear();
        }
        d->m_scalingFuture.waitForFinished();
        return 0;
    }

    void OpenGLWidgetRenderer::pause(bool pause) {
        Q_D(AVQt::OpenGLWidgetRenderer);
        d->m_isPaused.store(pause);
        paused(pause);
    }

    void OpenGLWidgetRenderer::onFrame(QImage frame, AVRational timebase, AVRational framerate, int64_t duration) {
        Q_D(AVQt::OpenGLWidgetRenderer);

        auto imageRect = frame.rect();

        double scale = std::min(width() * 1.0 / frame.width(), height() * 1.0 / frame.height());
        imageRect.setHeight(scale < 1.0 ? scale * frame.height() : frame.height());
        imageRect.setWidth(scale < 1.0 ? scale * frame.width() : frame.width());
        imageRect.translate((width() - imageRect.width()) / 2.0, (height() - imageRect.height()) / 2.0);
//        imageRect.setTop((rect().height() - imageRect.height()) / 2);
//        imageRect.setWidth((rect().width() - imageRect.width()) / 2);

        while (d->m_imageRenderQueue.size() >= 25) {
            QThread::msleep(duration);
        }
        {
            QMutexLocker lock(&d->m_imageRenderQueueMutex);
//            d->m_imageRenderQueue.enqueue(QPair<QRect, QImage>(imageRect, imageFrame));
            d->m_imageRenderQueue.enqueue({imageRect, frame, QVariant::fromValue(duration)});
        }
//        if (d->m_updateTimer) {
//            d->m_updateInterval = 1000.0 / av_q2d(framerate);
//        }
    }

    void OpenGLWidgetRenderer::onFrame(AVFrame *frame, AVRational timebase, AVRational framerate, int64_t duration) {
        Q_D(AVQt::OpenGLWidgetRenderer);


        auto imageRect = QRect(0, 0, frame->width, frame->height);
//        qDebug() << "Frame rect:" << imageRect;

        double scale = std::min(width() * 1.0 / frame->width, height() * 1.0 / frame->height);
        imageRect.setHeight(scale < 1.0 ? scale * frame->height : frame->height);
        imageRect.setWidth(scale < 1.0 ? scale * frame->width : frame->width);
        imageRect.translate((width() - imageRect.width()) / 2.0, (height() - imageRect.height()) / 2.0);
//        imageRect.setTop((rect().height() - imageRect.height()) / 2);
//        imageRect.setWidth((rect().width() - imageRect.width()) / 2);

        while (d->m_imageInputQueue.size() >= 25) {
            QThread::msleep(duration);
        }
        {
            QMutexLocker lock(&d->m_imageInputQueueMutex);
//            d->m_imageInputQueue.enqueue(QPair<QRect, QImage>(imageRect, imageFrame));
            AVFrame *frameRef = av_frame_alloc();
            av_frame_ref(frameRef, frame);
            d->m_imageInputQueue.enqueue({imageRect, QVariant::fromValue((uint64_t) frameRef), QVariant::fromValue(duration)});
        }
//        qDebug("Frame duration: %ld", duration);
//        qDebug("Framerate: %ld/%ld", framerate.num, framerate.den);
//        qDebug() << "Update interval:" << 1000.0 / av_q2d(framerate);
//        if (d->m_updateTimer) {
//            d->m_updateInterval = 1000.0 / av_q2d(framerate);
//        }
    }

    void OpenGLWidgetRendererPrivate::resizeFrameRects() {
        {
            QMutexLocker lock(&m_imageInputQueueMutex);
//            qDebug() << "Resizing frame rects";
            for (auto &f: m_imageInputQueue) {
                QRect newRect = reinterpret_cast<QImage *>(f[1].data())->rect();
                double scale = std::min(q_ptr->width() * 1.0 / newRect.width(), q_ptr->height() * 1.0 / newRect.height());
                newRect.setHeight(scale < 1.0 ? scale * newRect.height() : newRect.height());
                newRect.setWidth(scale < 1.0 ? scale * newRect.width() : newRect.width());
                f[0] = newRect;
            }
        }
//        {
//            QMutexLocker lock(&m_currentImageMutex);
//            QRect newRect = m_currentImage.rect();
//            double scale = std::min(q_ptr->width() * 1.0 / newRect.width(), q_ptr->height() * 1.0 / newRect.height());
//            newRect.setHeight(scale < 1.0 ? scale * newRect.height() : newRect.height());
//            newRect.setWidth(scale < 1.0 ? scale * newRect.width() : newRect.width());
//            m_currentImageRect = newRect;
//        }
    }

    void OpenGLWidgetRenderer::resizeEvent(QResizeEvent *event) {
        Q_D(AVQt::OpenGLWidgetRenderer);
        qDebug() << "Resize event";
//        QtConcurrent::run(&OpenGLWidgetRendererPrivate::resizeFrameRects, d);
        d->resizeFrameRects();
        QOpenGLWidget::resizeEvent(event);
    }

    void OpenGLWidgetRenderer::paintEvent(QPaintEvent *event) {
        Q_D(AVQt::OpenGLWidgetRenderer);
        auto t1 = NOW();
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setRenderHint(QPainter::LosslessImageRendering);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::black);
        if (!d->m_isPaused.load() && d->m_isRunning.load()) {
//        qDebug("Time since last paint: %ld", TIME_US(d->m_lastNewFrame, std::chrono::high_resolution_clock::now()));
//        d->m_lastNewFrame = NOW();

//        qDebug() << "Paint event";
            QMutexLocker imageLock(&d->m_currentImageMutex);
            auto t3 = NOW();
            if (d->m_imageRenderQueue.size() >= 5 /*&& TIME_US(d->m_lastNewFrame, t3) >= d->m_currentTimeout ||
            std::abs(TIME_US(d->m_lastNewFrame, t3) - d->m_currentTimeout) < 5*/) {
                qDebug("Time since last next frame: %ld us", TIME_US(d->m_lastNewFrame, t3));
                d->m_lastNewFrame = t3;
//            qDebug() << "New frame";
                QVariantList frame;
                {
                    QMutexLocker queueLock(&d->m_imageRenderQueueMutex);
                    frame = d->m_imageRenderQueue.dequeue();
                }
                d->m_currentImageRect = frame[0].toRect();
//            qDebug() << "Rect" << d->m_currentImageRect;
                d->m_currentImage = *reinterpret_cast<QImage *>(frame[1].data());
                d->m_currentTimeout = frame[2].toUInt();
            }

            if (d->m_currentImageRect.width() != width() && d->m_currentImageRect.height() != height()) {
                auto &imageRect = d->m_currentImageRect;
                double scale = std::min(width() * 1.0 / d->m_currentImage.width(), height() * 1.0 / d->m_currentImage.height());
                imageRect.setHeight(scale < 1.0 ? scale * d->m_currentImage.height() : d->m_currentImage.height());
                imageRect.setWidth(scale < 1.0 ? scale * d->m_currentImage.width() : d->m_currentImage.width());
                imageRect.translate((width() - imageRect.width()) / 2.0, (height() - imageRect.height()) / 2.0);
            }

//        if (!d->m_updateTimer) {
//            qDebug() << "Init timer";
//            d->m_updateTimer = new QTimer;
//            d->m_updateTimer->setInterval(d->m_currentTimeout);
//            connect(d->m_updateTimer, SIGNAL(timeout()), this, SLOT(update()));
//            d->m_updateTimer->start();
//        }
//
//        if (d->m_updateTimer->interval() != d->m_currentTimeout) {
//            d->m_updateTimer->setInterval(d->m_currentTimeout);
//        }

            p.drawImage(d->m_currentImageRect, d->m_currentImage);
        }
        auto t2 = NOW();
        qDebug("Render time: %ld us", TIME_US(t1, t2));
        QTimer::singleShot(d->m_currentTimeout - (TIME_US(t1, t2) / 1000.0), this, SLOT(update()));
    }

    bool OpenGLWidgetRenderer::isPaused() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        return !d->m_isPaused.load();
    }

    void OpenGLWidgetRenderer::run() {
        Q_D(AVQt::OpenGLWidgetRenderer);
        while (d->m_isRunning.load()) {
            QMutexLocker inputLock(&d->m_imageInputQueueMutex);
            if (!d->m_imageInputQueue.isEmpty()) {
                auto queueFrame = d->m_imageInputQueue.dequeue();
                auto frame = ((AVFrame *) *reinterpret_cast<uint64_t *>(queueFrame[1].data()));
                inputLock.unlock();
                QImage imageFrame;
                if (frame->format != AV_PIX_FMT_BGRA) {
                    if (d->m_pSwsContext) {
                        if (d->m_swsCtxFormat != static_cast<AVPixelFormat>(frame->format)) {
                            sws_freeContext(d->m_pSwsContext);
                            d->m_pSwsContext = nullptr;
                        }
                    }

                    if (!d->m_pSwsContext) {
                        d->m_pSwsContext = sws_getContext(frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                                                          frame->width,
                                                          frame->height, AV_PIX_FMT_BGRA, 0, nullptr, nullptr, nullptr);
                        d->m_swsCtxFormat = static_cast<AVPixelFormat>(frame->format);
                    }

                    AVFrame *frame1 = av_frame_alloc();
                    av_image_alloc(frame1->data, frame1->linesize, frame->width, frame->height, AV_PIX_FMT_BGRA, 1);

                    sws_scale(d->m_pSwsContext, frame->data, frame->linesize, 0, frame->height, frame1->data, frame1->linesize);

                    imageFrame = std::move(QImage(frame1->data[0], frame->width, frame->height, QImage::Format_ARGB32).copy());

                    av_freep(&frame1->data[0]);
                    av_frame_free(&frame1);
                } else {
                    imageFrame = std::move(QImage(frame->data[0], frame->width, frame->height, QImage::Format_ARGB32).copy());
                }
                while (d->m_imageRenderQueue.size() >= 25) {
                    QThread::msleep(queueFrame[2].toUInt());
                }
                QMutexLocker renderLock(&d->m_imageRenderQueueMutex);
                d->m_imageRenderQueue.enqueue({queueFrame[0], imageFrame, queueFrame[2]});
                av_frame_unref(frame);
            }
        }
    }
}
