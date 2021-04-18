//
// Created by silas on 4/18/21.
//

#include "private/EncoderVAAPI_p.h"
#include "EncoderVAAPI.h"

#include <QtCore>
#include "output/IPacketSink.h"

namespace AVQt {
    EncoderVAAPI::EncoderVAAPI(QString encoder, QObject *parent) : QThread(parent), d_ptr(new EncoderVAAPIPrivate(this)) {
        Q_D(AVQt::EncoderVAAPI);

        d->m_encoder = std::move(encoder);
    }

    EncoderVAAPI::EncoderVAAPI(EncoderVAAPIPrivate &p) : d_ptr(&p) {

    }

    EncoderVAAPI::~EncoderVAAPI() {
        delete d_ptr;
    };

    int EncoderVAAPI::init() {
        Q_D(AVQt::EncoderVAAPI);

        int ret = 0;
        constexpr auto strBufSize = 64;
        char strBuf[strBufSize];

        ret = av_hwdevice_ctx_create(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
        if (ret < 0) {
            qFatal("%i: Unable to create AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
        } else if (!d->m_pDeviceCtx) {
            qFatal("Unable to create AVHWDeviceContext");
        }

        d->m_pCodec = avcodec_find_encoder_by_name(d->m_encoder.toLocal8Bit().constData());
        if (!d->m_pCodec) {
            qFatal("Could not find encoder: %s", d->m_encoder.toLocal8Bit().constData());
        }

        return 0;
    }

    int EncoderVAAPI::deinit() {
        Q_D(AVQt::EncoderVAAPI);

        if (d->m_pDeviceCtx) {
            av_buffer_unref(&d->m_pDeviceCtx);
        }

        d->m_pCodec = nullptr;

        if (d->m_pCodecCtx) {
            if (avcodec_is_open(d->m_pCodecCtx)) {
                avcodec_close(d->m_pCodecCtx);
            }
            avcodec_free_context(&d->m_pCodecCtx);
        }

        if (d->m_pFramesCtx) {
            av_buffer_unref(&d->m_pFramesCtx);
        }

        return 0;
    }

    int EncoderVAAPI::start() {
        Q_D(AVQt::EncoderVAAPI);

        bool shouldBe = false;
        if (d->m_running.compare_exchange_strong(shouldBe, true)) {
            d->m_paused.store(false);
            QThread::start();

            started();
        }

        return 0;
    }

    int EncoderVAAPI::stop() {
        Q_D(AVQt::EncoderVAAPI);

        bool shouldBe = true;
        if (d->m_running.compare_exchange_strong(shouldBe, true)) {
            d->m_paused.store(false);

            {
                QMutexLocker lock{&d->m_cbListMutex};
                for (const auto &cb: d->m_cbList) {
                    cb->stop(this);
                }
            }

            wait();

            {
                QMutexLocker lock{&d->m_inputQueueMutex};
                while (!d->m_inputQueue.isEmpty()) {
                    auto frame = d->m_inputQueue.dequeue();
                    av_frame_free(&frame.first);
                }
            }

            stopped();
        }

        return 0;
    }

    void EncoderVAAPI::pause(bool pause) {
        Q_D(AVQt::EncoderVAAPI);

        bool shouldBe = !pause;
        if (d->m_paused.compare_exchange_strong(shouldBe, pause)) {
            paused(pause);
        }
    }

    bool EncoderVAAPI::isPaused() {
        Q_D(AVQt::EncoderVAAPI);
        return d->m_paused.load();
    }

    int EncoderVAAPI::init(IFrameSource *source, AVRational framerate, int64_t duration) {
        Q_UNUSED(source);
        Q_UNUSED(duration)
        Q_D(AVQt::EncoderVAAPI);
        d->m_framerate = framerate;
        init();
        return 0;
    }

    int EncoderVAAPI::deinit(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::EncoderVAAPI);
        deinit();
        return 0;
    }

    int EncoderVAAPI::start(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::EncoderVAAPI);
        start();
        return 0;
    }

    int EncoderVAAPI::stop(IFrameSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::EncoderVAAPI);
        stop();
        return 0;
    }

    void EncoderVAAPI::pause(IFrameSource *source, bool paused) {
        Q_UNUSED(source)
        Q_D(AVQt::EncoderVAAPI);
        this->pause(paused);
    }

    qsizetype EncoderVAAPI::registerCallback(IPacketSink *packetSink, uint8_t type) {
        Q_D(AVQt::EncoderVAAPI);

        if (type != IPacketSource::CB_VIDEO) {
            return -1;
        }

        {
            QMutexLocker lock{&d->m_cbListMutex};
            if (!d->m_cbList.contains(packetSink)) {
                d->m_cbList.append(packetSink);
                return d->m_cbList.indexOf(packetSink);
            } else {
                return -1;
            }
        }
    }

    qsizetype EncoderVAAPI::unregisterCallback(IPacketSink *packetSink) {
        Q_D(AVQt::EncoderVAAPI);

        {
            QMutexLocker lock{&d->m_cbListMutex};
            auto count = d->m_cbList.removeAll(packetSink);
            return count > 0 ? count : -1;
        }
    }

    void EncoderVAAPI::onFrame(IFrameSource *source, AVFrame *frame, int64_t frameDuration) {
        Q_UNUSED(source);
        Q_D(AVQt::EncoderVAAPI);

        QPair<AVFrame *, int64_t> queueFrame{av_frame_clone(frame), frameDuration};

        while (d->m_inputQueue.size() > 100) {
            QThread::msleep(3);
        }
        {
            QMutexLocker lock{&d->m_inputQueueMutex};
            d->m_inputQueue.enqueue(queueFrame);
        }
    }

    void EncoderVAAPI::run() {
        Q_D(AVQt::EncoderVAAPI);

        int ret {0};
        constexpr auto strBufSize {64};
        char strBuf[strBufSize];

        while (d->m_running.load()) {
            if (!d->m_paused.load() && !d->m_inputQueue.isEmpty()) {
                bool shouldBe = true;
                if (d->m_firstFrame.compare_exchange_strong(shouldBe, false)) {
                    auto frame = d->m_inputQueue.first().first;
                    d->m_pCodecCtx = avcodec_alloc_context3(d->m_pCodec);
                    d->m_pCodecCtx->width = frame->width;
                    d->m_pCodecCtx->height = frame->height;
                    d->m_pCodecCtx->sw_pix_fmt = static_cast<AVPixelFormat>(frame->format);
                    d->m_pCodecCtx->time_base = av_inv_q(d->m_framerate);
                }
                while (ret == 0) {
                    QPair<AVFrame *, int64_t> frame;
                    {
                        QMutexLocker lock(&d->m_inputQueueMutex);
                        frame = d->m_inputQueue.dequeue();
                    }
                    ret = avcodec_send_frame(d->m_pCodecCtx, frame.first);
                    av_frame_free(&frame.first);
                }
                ret = 0;
                AVPacket *packet = av_packet_alloc();
                while (true) {
                    ret = avcodec_receive_packet(d->m_pCodecCtx, packet);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret < 0) {
                        qFatal("%i: Could not receive frame from encoder: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                    {
                        QMutexLocker lock(&d->m_cbListMutex);
                        for (const auto &cb: d->m_cbList) {
                            AVPacket *cbPacket = av_packet_clone(packet);
                            cb->onPacket(this, cbPacket, IPacketSource::CB_VIDEO);
                            av_packet_free(&cbPacket);
                        }
                    }
                }
            } else {
                msleep(1);
            }
        }
    }
}