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
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

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
                goto failed;
            }

            if (0 != av_hwdevice_ctx_create(&d->hwDeviceContext, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0)) {
                qWarning() << "Could not create VAAPI device context";
                goto failed;
            }

            if (avcodec_parameters_to_context(d->codecContext, d->codecParams) < 0) {
                qWarning() << "Could not copy codec parameters to context";
                goto failed;
            }

            d->codecContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext);
            d->codecContext->get_format = &VAAPIDecoderImplPrivate::getFormat;

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
        qDebug() << "Stopping decoder";

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
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN) || ret == AVERROR(ENOMEM)) {
                return EAGAIN;
            } else if (ret < 0) {
                char strBuf[256];
                qWarning("Could not send packet to codec: %s", av_make_error_string(strBuf, sizeof(strBuf), ret));
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
        return d->frameFetcher->nextFrame();
    }

    AVPixelFormat VAAPIDecoderImpl::getOutputFormat() const {
        return AV_PIX_FMT_VAAPI;
    }

    bool VAAPIDecoderImpl::isHWAccel() const {
        return true;
    }
    AVRational VAAPIDecoderImpl::getTimeBase() const {
        Q_D(const VAAPIDecoderImpl);
        if (!d->codecContext) {
            qWarning() << "Codec context not initialized";
            return AVRational{0, 1};
        }
        return d->codecContext->time_base;
    }

    AVPixelFormat VAAPIDecoderImplPrivate::getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts) {
        Q_UNUSED(ctx)
        auto *iFmt = pix_fmts;
        AVPixelFormat result = AV_PIX_FMT_NONE;
        while (*iFmt != AV_PIX_FMT_NONE) {
            if (*iFmt == AV_PIX_FMT_VAAPI) {
                result = *iFmt;
                break;
            }
            ++iFmt;
        }

        //        ctx->hw_frames_ctx = av_hwframe_ctx_alloc(ctx->hw_device_ctx);
        //        auto framesContext = reinterpret_cast<AVHWFramesContext *>(ctx->hw_frames_ctx->data);
        //        framesContext->width = ctx->width;
        //        framesContext->height = ctx->height;
        //        framesContext->format = result;
        //        framesContext->sw_format = ctx->sw_pix_fmt;
        //        framesContext->initial_pool_size = 100;
        //        int ret = av_hwframe_ctx_init(ctx->hw_frames_ctx);
        //        if (ret != 0) {
        //            char strBuf[64];
        //            qFatal("[AVQt::DecoderVAAPI] %d: Could not initialize HW frames context: %s", ret, av_make_error_string(strBuf, 64, ret));
        //        }
        return result;
    }

    VAAPIDecoderImplPrivate::FrameFetcher::FrameFetcher(VAAPIDecoderImplPrivate *p) : p(p) {
    }

    void VAAPIDecoderImplPrivate::FrameFetcher::run() {
        qDebug() << "FrameFetcher started";
        int ret;
        constexpr size_t strBufSize = 256;
        char strBuf[strBufSize];
        while (!m_stop) {
            QMutexLocker locker(&p->codecMutex);
            if (p->firstFrame) {
                locker.unlock();
                msleep(2);
                continue;
            }
            AVFrame *frame = av_frame_alloc();
            ret = avcodec_receive_frame(p->codecContext, frame);
            locker.unlock();
            if (ret == 0) {
                QMutexLocker lock(&m_mutex);
                m_outputQueue.enqueue(frame);
            } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                locker.unlock();
                av_frame_free(&frame);
                msleep(1);
            } else {
                av_frame_free(&frame);
                av_strerror(ret, strBuf, strBufSize);
                qFatal("Error while receiving frame: %s", strBuf);
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
            {
                QMutexLocker locker(&m_mutex);
                while (!m_outputQueue.isEmpty()) {
                    AVFrame *frame = m_outputQueue.dequeue();
                    av_frame_free(&frame);
                }
            }
        }
    }

    AVFrame *VAAPIDecoderImplPrivate::FrameFetcher::nextFrame() {
        QMutexLocker lock(&m_mutex);
        if (m_outputQueue.isEmpty()) {
            return nullptr;
        }
        return m_outputQueue.dequeue();
    }

    bool VAAPIDecoderImplPrivate::FrameFetcher::isEmpty() const {
        return m_outputQueue.isEmpty();
    }
}// namespace AVQt
