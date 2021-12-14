// Copyright (c) 2021.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BELIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORTOR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OROTHER DEALINGS IN THE SOFTWARE.

//
// Created by silas on 12.12.21.
//

#include "include/AVQt/decoder/VAAPIDecoderImpl.hpp"
#include "private/VAAPIDecoderImpl_p.hpp"

#include <QImage>
#include <QMetaType>
#include <QtDebug>

extern "C" {
#include <libavutil/time.h>
}

namespace AVQt {
    VAAPIDecoderImpl::VAAPIDecoderImpl() : QObject(nullptr), d_ptr(new VAAPIDecoderImplPrivate(this)) {
    }

    bool VAAPIDecoderImpl::open(AVCodecParameters *codecParams) {
        Q_D(VAAPIDecoderImpl);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            d->codecParams = avcodec_parameters_alloc();
            avcodec_parameters_copy(d->codecParams, codecParams);

            d->codec = avcodec_find_decoder(d->codecParams->codec_id);
            if (!d->codec) {
                qWarning() << "Could not find decoder for codec id" << d->codecParams->codec_id;
                d->initialized = false;
                goto failed;
            }

            d->codecContext = avcodec_alloc_context3(d->codec);
            if (!d->codecContext) {
                qWarning() << "Could not allocate codec context";
                d->initialized = false;
                goto failed;
            }

            if (avcodec_parameters_to_context(d->codecContext, d->codecParams) < 0) {
                qWarning() << "Could not copy codec parameters to context";
                d->initialized = false;
                goto failed;
            }

            if (0 != av_hwdevice_ctx_create(&d->hwDeviceContext, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0)) {
                qWarning() << "Could not create VAAPI device context";
                goto failed;
            }

            d->codecContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext);
            d->codecContext->get_format = &VAAPIDecoderImplPrivate::getFormat;
            d->codecContext->pix_fmt = AV_PIX_FMT_VAAPI;

            if (avcodec_open2(d->codecContext, d->codec, nullptr) < 0) {
                qWarning() << "Could not open codec";
                goto failed;
            }

            d->frameFetcher = new VAAPIDecoderImplPrivate::FrameFetcher(d);
            d->frameFetcher->start();

            return true;
        }
        return false;

    failed:
        close();
        d->initialized = false;
        return false;
    }

    void VAAPIDecoderImpl::close() {
        Q_D(VAAPIDecoderImpl);

        d->frameFetcher->stop();

        if (d->codecContext) {
            if (avcodec_is_open(d->codecContext)) {
                avcodec_close(d->codecContext);
            }
            avcodec_free_context(&d->codecContext);
        }
        if (d->hwDeviceContext) {
            av_buffer_unref(&d->hwDeviceContext);
        }
        d->codec = nullptr;
    }

    int VAAPIDecoderImpl::decode(AVPacket *packet) {
        Q_D(VAAPIDecoderImpl);
        if (!d->codecContext) {
            qWarning() << "Codec context not initialized";
            return ENODEV;
        }

        {
            QMutexLocker locker(&d->codecMutex);
            int ret = avcodec_send_packet(d->codecContext, packet);
            locker.unlock();
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                return EAGAIN;
            } else if (ret < 0) {
                char strBuf[256];
                qWarning() << "Could not send packet to codec" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                return AVUNERROR(ret);
            }
        }

        d->firstFrame = false;

        return EXIT_SUCCESS;
    }
    AVFrame *VAAPIDecoderImpl::nextFrame() {
        Q_D(VAAPIDecoderImpl);
        if (!d->initialized) {
            qWarning() << "Decoder not initialized";
            return nullptr;
        }
        if (d->frameFetcher->isEmpty()) {
            return nullptr;
        }
        return d->frameFetcher->nextFrame();
    }

    AVPixelFormat VAAPIDecoderImpl::getOutputFormat() const {
        return AV_PIX_FMT_VAAPI;
    }

    bool VAAPIDecoderImpl::isHWAccel() const {
        return true;
    }

    AVPixelFormat VAAPIDecoderImplPrivate::getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts) {
        for (int i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
            if (pix_fmts[i] == AV_PIX_FMT_VAAPI) {
                return AV_PIX_FMT_VAAPI;
            }
        }
        return AV_PIX_FMT_NONE;
    }

    VAAPIDecoderImplPrivate::FrameFetcher::FrameFetcher(VAAPIDecoderImplPrivate *p) : p(p) {
    }

    void VAAPIDecoderImplPrivate::FrameFetcher::run() {
        qDebug() << "FrameFetcher started";
        int ret;
        constexpr size_t strBufSize = 256;
        char strBuf[strBufSize];
        while (!m_stop) {
            if (p->firstFrame) {
                av_usleep(1000);
                continue;
            }
            AVFrame *frame = av_frame_alloc();
            {
                QMutexLocker locker(&p->codecMutex);
                ret = avcodec_receive_frame(p->codecContext, frame);
            }
            if (ret == 0) {
                m_outputQueue.enqueue(frame);
            } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_usleep(1000);
            } else {
                av_strerror(ret, strBuf, strBufSize);
                qWarning() << "Error while receiving frame: " << strBuf;
                m_stop = true;
                break;
            }
        }
        qDebug() << "FrameFetcher stopped";
    }

    void VAAPIDecoderImplPrivate::FrameFetcher::stop() {
        if (isRunning()) {
            m_stop = true;
            QThread::quit();
            QThread::wait();
        }
    }

    AVFrame *VAAPIDecoderImplPrivate::FrameFetcher::nextFrame() {
        QMutexLocker lock(&m_mutex);
        return m_outputQueue.dequeue();
    }

    bool VAAPIDecoderImplPrivate::FrameFetcher::isEmpty() const {
        return m_outputQueue.isEmpty();
    }
}// namespace AVQt
