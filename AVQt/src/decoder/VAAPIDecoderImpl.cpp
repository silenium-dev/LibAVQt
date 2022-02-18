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
}

namespace AVQt {
    const api::VideoDecoderInfo VAAPIDecoderImpl::info{
            .metaObject = VAAPIDecoderImpl::staticMetaObject,
            .name = "VAAPI",
            .platforms = {common::Platform::Linux_X11, common::Platform::Linux_Wayland},
            .supportedInputPixelFormats = {
                    {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_YUV420P10, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_YUV420P12, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_YUV420P14, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_YUV420P16, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_NV12, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_P010, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_P016, AV_PIX_FMT_NONE},
            },
            .supportedCodecIds = {
                    AV_CODEC_ID_H264,
                    AV_CODEC_ID_HEVC,
                    AV_CODEC_ID_VP8,
                    AV_CODEC_ID_VP9,
                    AV_CODEC_ID_MPEG2VIDEO,
                    AV_CODEC_ID_NONE,
            }};

    VAAPIDecoderImpl::VAAPIDecoderImpl(AVCodecID codec) : QObject(nullptr), d_ptr(new VAAPIDecoderImplPrivate(this)) {
        Q_D(VAAPIDecoderImpl);
        d->frameDestructor = std::make_shared<internal::FrameDestructor>();
        d->codec = avcodec_find_decoder(codec);
        if (!d->codec) {
            qWarning() << "Failed to find decoder for codec" << avcodec_get_name(codec);
            return;
        }
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

            if (!d->codec) {
                qWarning() << "Failed to find decoder for codec";
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
                framesContext->initial_pool_size = 48;
                int ret = av_hwframe_ctx_init(hwFramesContext);
                if (ret != 0) {
                    char strBuf[64];
                    qWarning("[AVQt::VAAPIDecoderImpl] %d: Could not initialize HW frames context: %s", ret, av_make_error_string(strBuf, 64, ret));
                    av_buffer_unref(&hwFramesContext);
                    goto failed;
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

            //            connect(this, &VAAPIDecoderImpl::triggerFetchFrames, d->frameFetcher.get(), &VAAPIDecoderImplPrivate::FrameFetcher::fetchFrames, Qt::DirectConnection);

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

        int ret = AVERROR(EXIT_SUCCESS);

        {
            QMutexLocker lock(&d->codecMutex);
            ret = avcodec_send_packet(d->codecContext.get(), packet.get());
        }
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            char errBuf[AV_ERROR_MAX_STRING_SIZE];
            qWarning() << "Could not send packet to decoder:" << av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
        }

        d->firstFrame = false;

        return AVUNERROR(ret);
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
        setObjectName("AVQt::VAAPIDecoderImplPrivate::FrameFetcher");
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
            if (ret == 0) {
                //                auto t = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                //                qWarning("Frame decoded at: %ld us since epoch", t);
                //                frame->opaque = reinterpret_cast<void *>(t);
                //                frame->pts = av_rescale_q(frame->pts, p->timeBase, av_make_q(1, 1000000));
                qDebug("Publishing frame with PTS %lld", frame->pts);
                QElapsedTimer timer;
                timer.start();
                p->q_func()->frameReady(frame);
                inputLock.unlock();
                //                qDebug("Frame ref count: %ld", sharedFrame.use_count());
                qDebug("Frame callback runtime: %lld ms", timer.elapsed());
                //                m_outputQueue.enqueue(frame);
            } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret == AVERROR(ENOMEM)) {
                frame.reset();
                inputLock.unlock();
                msleep(4);
            } else {
                inputLock.unlock();
                av_strerror(ret, strBuf, strBufSize);
                qWarning("Error while receiving frame: %s", strBuf);
                m_stop = true;
                break;
            }
        }
        //        exec();
        //        if (m_afterStopThread) {
        //            moveToThread(m_afterStopThread);
        //        }
        qDebug() << "FrameFetcher stopped";
    }

    //    void VAAPIDecoderImplPrivate::FrameFetcher::fetchFrames() {
    //        int fetchRet;
    //        while (true) {
    //            std::shared_ptr<AVFrame> frame{av_frame_alloc(), *p->frameDestructor};
    //            {
    //                QMutexLocker lock(&p->codecMutex);
    //                fetchRet = avcodec_receive_frame(p->codecContext.get(), frame.get());
    //            }
    //            if (fetchRet == 0) {
    //                p->q_func()->frameReady(frame);
    //            } else if (fetchRet == AVERROR(EAGAIN)) {
    //                break;
    //            } else if (fetchRet == AVERROR_EOF) {
    //                qDebug() << "End of stream";
    //                break;
    //            } else {
    //                char strBuf[AV_ERROR_MAX_STRING_SIZE];
    //                qWarning() << "Could not receive frame:" << av_make_error_string(strBuf, AV_ERROR_MAX_STRING_SIZE, fetchRet);
    //                break;
    //            }
    //        }
    //    }

    void VAAPIDecoderImplPrivate::FrameFetcher::stop() {
        if (isRunning()) {
            m_stop = true;
            QThread::quit();
            QThread::wait();
        } else {
            qWarning("FrameFetcher not running");
        }
    }

    void VAAPIDecoderImplPrivate::FrameFetcher::start() {
        if (!isRunning()) {
            m_stop = false;
            //            moveToThread(this);
            QThread::start();
        } else {
            qWarning("FrameFetcher already running");
        }
    }
}// namespace AVQt
