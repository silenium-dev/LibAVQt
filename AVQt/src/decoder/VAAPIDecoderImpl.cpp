// Copyright (c) 2021-2022.
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

#include "VAAPIDecoderImpl.hpp"
#include "private/VAAPIDecoderImpl_p.hpp"

#include <QImage>
#include <QMetaType>
#include <QtDebug>

extern "C" {
#include <libavutil/pixdesc.h>
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
            d->codecContext->opaque = this;

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

        if (d->frameFetcher) {
            d->frameFetcher->stop();
        }

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

    AVPixelFormat VAAPIDecoderImpl::getSwOutputFormat() const {
        Q_D(const VAAPIDecoderImpl);
        AVPixelFormat format;
        if (d->codecContext->pix_fmt == AV_PIX_FMT_VAAPI) {
            format = d->codecContext->sw_pix_fmt;
        } else {
            format = d->codecContext->pix_fmt;
        }
        switch (format) {
            case AV_PIX_FMT_NV12:
            case AV_PIX_FMT_YUV420P:
                return AV_PIX_FMT_NV12;
            case AV_PIX_FMT_P010:
            case AV_PIX_FMT_YUV420P10:
                return AV_PIX_FMT_P010;
            default:
                qWarning() << "Unsupported output format" << av_get_pix_fmt_name(format);
                return AV_PIX_FMT_NONE;
        }
    }

    bool VAAPIDecoderImpl::isHWAccel() const {
        return true;
    }

    VideoPadParams VAAPIDecoderImpl::getVideoParams() const {
        Q_D(const VAAPIDecoderImpl);
        VideoPadParams params;
        params.frameSize = QSize{d->codecContext->width, d->codecContext->height};
        params.hwDeviceContext = av_buffer_ref(d->hwDeviceContext);
        //        params.hwFramesContext = av_buffer_ref(d->codecContext->hw_frames_ctx);
        params.swPixelFormat = getSwOutputFormat();
        params.pixelFormat = getOutputFormat();
        params.isHWAccel = isHWAccel();
        return params;
    }

    AVPixelFormat VAAPIDecoderImplPrivate::getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts) {
        auto *decoder = reinterpret_cast<VAAPIDecoderImpl *>(ctx->opaque);
        auto *iFmt = pix_fmts;
        AVPixelFormat result = AV_PIX_FMT_NONE;
        while (*iFmt != AV_PIX_FMT_NONE) {
            if (*iFmt == AV_PIX_FMT_VAAPI) {
                result = *iFmt;
                break;
            }
            ++iFmt;
        }
        if (result == AV_PIX_FMT_NONE) {
            qWarning() << "No supported output format found";
            return AV_PIX_FMT_NONE;
        }

        ctx->hw_frames_ctx = av_hwframe_ctx_alloc(ctx->hw_device_ctx);
        auto framesContext = reinterpret_cast<AVHWFramesContext *>(ctx->hw_frames_ctx->data);
        framesContext->width = ctx->width;
        framesContext->height = ctx->height;
        framesContext->format = AV_PIX_FMT_VAAPI;
        framesContext->sw_format = decoder->getSwOutputFormat();
        framesContext->initial_pool_size = 16;
        int ret = av_hwframe_ctx_init(ctx->hw_frames_ctx);
        if (ret != 0) {
            char strBuf[64];
            qFatal("[AVQt::DecoderVAAPI] %d: Could not initialize HW frames context: %s", ret, av_make_error_string(strBuf, 64, ret));
        }
        qDebug("[AVQt::DecoderVAAPI] Frame pool size: %d", framesContext->initial_pool_size);
        return result;
    }

    VAAPIDecoderImplPrivate::FrameFetcher::FrameFetcher(VAAPIDecoderImplPrivate *p) : p(p) {
        setObjectName("FrameFetcher");
    }

    void VAAPIDecoderImplPrivate::FrameFetcher::run() {
        qDebug() << "FrameFetcher started";
        int ret;
        constexpr size_t strBufSize = 256;
        char strBuf[strBufSize];
        while (!m_stop) {
            QMutexLocker inputLock(&p->codecMutex);
            if (p->firstFrame) {
                inputLock.unlock();
                msleep(2);
                continue;
            }
            AVFrame *frame = av_frame_alloc();
            ret = avcodec_receive_frame(p->codecContext, frame);
            inputLock.unlock();
            qWarning("Output queue size: %d", m_outputQueue.size());
            if (ret == 0) {
                QMutexLocker outputLock(&m_mutex);
                //                frame->pts = av_rescale_q(frame->pts, p->timeBase, av_make_q(1, 1000000));
                qDebug("Enqueuing frame with PTS %ld", frame->pts);
                m_outputQueue.enqueue(frame);
            } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret == AVERROR(ENOMEM)) {
                av_frame_free(&frame);
                msleep(4);
            } else {
                av_frame_free(&frame);
                av_strerror(ret, strBuf, strBufSize);
                qWarning("Error while receiving frame: %s", strBuf);
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
        } else {
            qWarning("FrameFetcher not running");
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
