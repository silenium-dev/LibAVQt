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
#include "communication/HwContextSync.hpp"
#include "private/VAAPIDecoderImpl_p.hpp"

#include <QImage>
#include <QMetaType>
#include <QtDebug>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace AVQt {
    VAAPIDecoderImpl::VAAPIDecoderImpl() : QObject(nullptr), d_ptr(new VAAPIDecoderImplPrivate(this)) {
        Q_D(VAAPIDecoderImpl);
        d->frameDestructor = std::make_shared<internal::FrameDestructor>();
    }

    VAAPIDecoderImpl::~VAAPIDecoderImpl() {
        Q_D(VAAPIDecoderImpl);
        if (d->initialized) {
            VAAPIDecoderImpl::close();
        }
    }

    bool VAAPIDecoderImpl::open(std::shared_ptr<AVCodecParameters> codecParams) {
        Q_D(VAAPIDecoderImpl);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            d->codecParams = codecParams;

            d->codec = avcodec_find_decoder(d->codecParams->codec_id);
            if (!d->codec) {
                qWarning() << "Could not find decoder for encoder id" << d->codecParams->codec_id;
                d->initialized = false;
                goto failed;
            }

            d->codecContext = {avcodec_alloc_context3(d->codec), &VAAPIDecoderImplPrivate::destroyAVCodecContext};
            if (!d->codecContext) {
                qWarning() << "Could not allocate encoder context";
                goto failed;
            }

            AVBufferRef *deviceContext;
            if (0 != av_hwdevice_ctx_create(&deviceContext, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0)) {
                qWarning() << "Could not create VAAPI device context";
                goto failed;
            }
            d->hwDeviceContext = {deviceContext, &VAAPIDecoderImplPrivate::destroyAVBufferRef};

            {
                auto hwFramesContext = av_hwframe_ctx_alloc(d->hwDeviceContext.get());
                auto framesContext = reinterpret_cast<AVHWFramesContext *>(hwFramesContext->data);
                framesContext->width = codecParams->width;
                framesContext->height = codecParams->height;
                framesContext->format = AV_PIX_FMT_VAAPI;
                framesContext->sw_format = getSwOutputFormat(codecParams->format);
                framesContext->initial_pool_size = 32;
                int ret = av_hwframe_ctx_init(hwFramesContext);
                if (ret != 0) {
                    char strBuf[64];
                    qFatal("[AVQt::VAAPIDecoderImpl] %d: Could not initialize HW frames context: %s", ret, av_make_error_string(strBuf, 64, ret));
                }
                d->hwFramesContext = {hwFramesContext, &VAAPIDecoderImplPrivate::destroyAVBufferRef};
            }

            if (avcodec_parameters_to_context(d->codecContext.get(), d->codecParams.get()) < 0) {
                qWarning() << "Could not copy encoder parameters to context";
                goto failed;
            }

            d->codecContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext.get());
            d->codecContext->get_format = &VAAPIDecoderImplPrivate::getFormat;
            d->codecContext->opaque = this;

            if (avcodec_open2(d->codecContext.get(), d->codec, nullptr) < 0) {
                qWarning() << "Could not open encoder";
                goto failed;
            }

            d->frameFetcher = std::make_unique<VAAPIDecoderImplPrivate::FrameFetcher>(d);
            d->frameFetcher->start();

            return true;
        }
        return false;

    failed:
        close();
        return false;
    }

    void VAAPIDecoderImpl::close() {
        Q_D(VAAPIDecoderImpl);
        qDebug() << "Stopping decoder";

        if (d->frameFetcher) {
            d->frameFetcher->stop();
            d->frameFetcher->wait();
            d->frameFetcher.reset();
        }

        d->codecContext.reset();
        d->codecParams.reset();

        d->hwDeviceContext.reset();
        d->hwFramesContext.reset();

        d->codec = nullptr;
    }

    int VAAPIDecoderImpl::decode(std::shared_ptr<AVPacket> packet) {
        Q_D(VAAPIDecoderImpl);
        if (!d->codecContext) {
            qWarning() << "Codec context not initialized";
            return ENODEV;
        }

        {
            //            HWContextSync *hwFramesMutex{nullptr};
            //            if (d->encodeContext->hw_frames_ctx) {
            //                hwFramesMutex = reinterpret_cast<HWContextSync *>(reinterpret_cast<AVHWFramesContext *>(d->encodeContext->hw_frames_ctx)->user_opaque);
            //            }
            int ret;
            {
                QMutexLocker locker(&d->codecMutex);
                //                if (hwFramesMutex) {
                {
                    //                        QMutexLocker hwFramesLocker(hwFramesMutex);
                    //                        qDebug("[AVQt::VAAPIDecoderImpl] Trying to lock hardware frames mutex: %p", hwFramesMutex);
                    //                        hwFramesMutex->lock();
                    //                        qDebug("[AVQt::VAAPIDecoderImpl] Using hardware frames mutex: %p", hwFramesMutex);
                    ret = avcodec_send_packet(d->codecContext.get(), packet.get());
                    //                        hwFramesMutex->unlock();
                }
                //                    qDebug("[AVQt::VAAPIDecoderImpl] Unlocked frame pool mutex: %p", hwFramesMutex);
                //                } else {
                //                    ret = avcodec_send_packet(d->encodeContext, packet);
                //                }
            }
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN) || ret == AVERROR(ENOMEM)) {
                return EAGAIN;
            } else if (ret < 0) {
                char strBuf[256];
                qWarning("Could not send packet to encoder: %s", av_make_error_string(strBuf, sizeof(strBuf), ret));
                return AVUNERROR(ret);
            }
        }

        d->firstFrame = false;

        return EXIT_SUCCESS;
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

    communication::VideoPadParams VAAPIDecoderImpl::getVideoParams() const {
        Q_D(const VAAPIDecoderImpl);
        communication::VideoPadParams params;
        params.frameSize = QSize{d->codecContext->width, d->codecContext->height};
        params.hwDeviceContext = d->hwDeviceContext;
        params.hwFramesContext = d->hwFramesContext;
        params.swPixelFormat = getSwOutputFormat();
        params.pixelFormat = getOutputFormat();
        params.isHWAccel = isHWAccel();
        return params;
    }

    AVPixelFormat VAAPIDecoderImpl::getSwOutputFormat(AVPixelFormat format) {
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

    AVPixelFormat VAAPIDecoderImpl::getSwOutputFormat(int format) {
        return getSwOutputFormat(static_cast<AVPixelFormat>(format));
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

        ctx->hw_frames_ctx = av_buffer_ref(decoder->d_func()->hwFramesContext.get());
        //        qDebug("[AVQt::VAAPIDecoderImpl] Frame pool size: %d", framesContext->initial_pool_size);
        //        framesContext->user_opaque = new HWContextSync(ctx->hw_frames_ctx);
        //        qDebug("[AVQt::VAAPIDecoderImpl] Frame pool mutex: %p", framesContext->user_opaque);
        //        framesContext->free = [](AVHWFramesContext *hwfc) {
        //            qDebug("[AVQt::VAAPIDecoderImpl] Frame pool free");
        //            auto *sync = reinterpret_cast<HWContextSync *>(hwfc->user_opaque);
        //            delete sync;
        //            hwfc->user_opaque = nullptr;
        //        };
        return result;
    }

    void VAAPIDecoderImplPrivate::destroyAVBufferRef(AVBufferRef *buffer) {
        if (buffer) {
            av_buffer_unref(&buffer);
        }
    }

    void VAAPIDecoderImplPrivate::destroyAVCodecContext(AVCodecContext *codecContext) {
        if (codecContext) {
            if (avcodec_is_open(codecContext)) {
                avcodec_close(codecContext);
            }
            avcodec_free_context(&codecContext);
        }
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
            std::shared_ptr<AVFrame> frame = {av_frame_alloc(), [](AVFrame *frame) {
                                                  av_frame_free(&frame);
                                              }};
            ret = avcodec_receive_frame(p->codecContext.get(), frame.get());
            //            qDebug("[AVQt::VAAPIDecoderImpl::Fetcher] Unlocked frame pool mutex: %p", hwFramesMutex);
            inputLock.unlock();
            if (ret == 0) {
                //                auto t = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                //                qWarning("Frame decoded at: %ld us since epoch", t);
                //                frame->opaque = reinterpret_cast<void *>(t);
                //                frame->pts = av_rescale_q(frame->pts, p->timeBase, av_make_q(1, 1000000));
                qDebug("Publishing frame with PTS %ld", frame->pts);
                QElapsedTimer timer;
                timer.start();
                p->q_func()->frameReady(frame);
                //                qDebug("Frame ref count: %ld", sharedFrame.use_count());
                qDebug("Frame callback runtime: %lld ms", timer.elapsed());
                //                m_outputQueue.enqueue(frame);
            } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret == AVERROR(ENOMEM)) {
                frame.reset();
                msleep(4);
            } else {
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
        } else {
            qWarning("FrameFetcher not running");
        }
    }
}// namespace AVQt
