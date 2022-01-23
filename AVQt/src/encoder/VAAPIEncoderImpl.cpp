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
// Created by silas on 28.12.21.
//

#include "VAAPIEncoderImpl.hpp"
#include "communication/HwContextSync.hpp"
#include "encoder/Encoder.hpp"
#include "private/VAAPIEncoderImpl_p.hpp"

#include <QImage>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace AVQt {
    const QList<AVPixelFormat> VAAPIEncoderImplPrivate::supportedPixelFormats{
            AV_PIX_FMT_NV12,
            AV_PIX_FMT_P010,
            AV_PIX_FMT_YUV420P,
            AV_PIX_FMT_YUV420P10,
            AV_PIX_FMT_QSV,
            AV_PIX_FMT_VAAPI};

    VAAPIEncoderImpl::VAAPIEncoderImpl(const EncodeParameters &parameters)
        : QObject(),
          IEncoderImpl(parameters),
          d_ptr(new VAAPIEncoderImplPrivate(this)) {
        Q_D(VAAPIEncoderImpl);

        QString codec;
        switch (parameters.codec) {
            case Codec::H264:
                codec = "h264_vaapi";
                break;
            case Codec::HEVC:
                codec = "hevc_vaapi";
                break;
            case Codec::VP8:
                codec = "vp8_vaapi";
                break;
            case Codec::VP9:
                codec = "vp9_vaapi";
                break;
            case Codec::MPEG2:
                codec = "mpeg2_vaapi";
                break;
        }

        d->codec = avcodec_find_encoder_by_name(qPrintable(codec));
        if (!d->codec) {
            qWarning() << "Codec not found";
            return;
        }
        d->encodeParameters = parameters;
        d->packetDestructor = std::make_shared<internal::PacketDestructor>();
    }

    VAAPIEncoderImpl::~VAAPIEncoderImpl() {
        VAAPIEncoderImpl::close();
        delete d_ptr;
        d_ptr = nullptr;
    }

    bool VAAPIEncoderImpl::open(const VideoPadParams &params) {
        Q_D(VAAPIEncoderImpl);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            int ret;
            char strBuf[256];
            bool createContext = false;

            d->codecContext = avcodec_alloc_context3(d->codec);
            if (!d->codecContext) {
                qWarning() << "Could not allocate video encoder context";
                return false;
            }

            if (params.hwDeviceContext && AVQt::VAAPIEncoderImplPrivate::supportedPixelFormats.contains(params.pixelFormat)) {
                d->derivedContext = true;
            }
            if (params.pixelFormat != AV_PIX_FMT_VAAPI) {
                if (!AVQt::VAAPIEncoderImplPrivate::supportedPixelFormats.contains(params.pixelFormat) &&
                    !AVQt::VAAPIEncoderImplPrivate::supportedPixelFormats.contains(params.swPixelFormat)) {
                    qWarning() << "Unsupported pixel formats:" << av_get_pix_fmt_name(params.pixelFormat)
                               << "and" << av_get_pix_fmt_name(params.swPixelFormat);
                    goto fail;
                }
                createContext = true;
            } else if (params.hwDeviceContext) {
                createContext = false;
                d->derivedContext = false;
                d->hwDeviceContext = av_buffer_ref(params.hwDeviceContext);
                d->hwFramesContext = av_buffer_ref(params.hwFramesContext);
                //                //                d->hwDeviceContext = av_buffer_ref(params.hwDeviceContext);
                //                ret = av_hwdevice_ctx_create(&d->hwDeviceContext, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
                //                if (ret < 0) {
                //                    qWarning() << "Could not create VAAPI device context";
                //                    goto fail;
                //                }
            }

            if (createContext) {
                if (d->derivedContext) {
                    ret = av_hwdevice_ctx_create_derived(&d->hwDeviceContext, AV_HWDEVICE_TYPE_VAAPI, params.hwDeviceContext, 0);
                    if (ret < 0) {
                        qWarning() << "Could not create derived device context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                        goto fail;
                    }
                } else {
                    ret = av_hwdevice_ctx_create(&d->hwDeviceContext, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
                    if (ret < 0) {
                        qWarning() << "Could not create device context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                        goto fail;
                    }
                    d->hwFramesContext = av_hwframe_ctx_alloc(d->hwDeviceContext);
                    if (!d->hwFramesContext) {
                        qWarning() << "Could not create hw frame context";
                        goto fail;
                    }
                    auto *hwFramesContext = reinterpret_cast<AVHWFramesContext *>(d->hwFramesContext->data);
                    hwFramesContext->format = AV_PIX_FMT_VAAPI;
                    hwFramesContext->sw_format = params.swPixelFormat;
                    hwFramesContext->width = params.frameSize.width();
                    hwFramesContext->height = params.frameSize.height();
                    hwFramesContext->initial_pool_size = 32;

                    ret = av_hwframe_ctx_init(d->hwFramesContext);
                    if (ret < 0) {
                        qWarning() << "Could not initialize hw frame context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                        goto fail;
                    }
                }
            }

            d->codecContext->pix_fmt = params.pixelFormat;
            d->codecContext->sw_pix_fmt = params.swPixelFormat;
            d->codecContext->width = params.frameSize.width();
            d->codecContext->height = params.frameSize.height();
            d->codecContext->max_b_frames = 0;
            d->codecContext->gop_size = 100;
            d->codecContext->time_base = {1, 1000000};// microseconds
            d->codecContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext);
            d->codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext);

            ret = avcodec_open2(d->codecContext, d->codec, nullptr);
            if (ret < 0) {
                qWarning() << "Could not open codec:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                goto fail;
            }

            d->packetFetcher = new internal::PacketFetcher(d);
            if (!d->packetFetcher) {
                qWarning() << "Could not create packet fetcher";
                goto fail;
            }
            d->packetFetcher->start();

            return true;
        } else {
            qWarning() << "Already initialized";
            return false;
        }
    fail:
        close();
        d->initialized = false;
        return EXIT_FAILURE;
    }

    void VAAPIEncoderImpl::close() {
        Q_D(VAAPIEncoderImpl);
        bool shouldBe = true;
        if (d->initialized.compare_exchange_strong(shouldBe, false)) {
            if (d->packetFetcher) {
                d->packetFetcher->stop();
                d->packetFetcher->wait();
                delete d->packetFetcher;
                d->packetFetcher = nullptr;
            }
            if (d->hwFrame) {
                av_frame_free(&d->hwFrame);
                d->hwFrame = nullptr;
            }
            if (d->codecContext) {
                avcodec_close(d->codecContext);
                avcodec_free_context(&d->codecContext);
                d->codecContext = nullptr;
            }
            if (d->hwFramesContext) {
                av_buffer_unref(&d->hwFramesContext);
                d->hwFramesContext = nullptr;
            }
            if (d->hwDeviceContext) {
                av_buffer_unref(&d->hwDeviceContext);
                d->hwDeviceContext = nullptr;
            }
        }
    }

    int VAAPIEncoderImpl::encode(AVFrame *frame) {
        Q_D(VAAPIEncoderImpl);

        int ret = 0;
        char strBuf[256];

        if (!d->codecContext) {
            qWarning() << "Codec context not allocated";
            return ENODEV;
        }

        if (frame == nullptr) {
            qWarning() << "Frame is nullptr";
            return ENODATA;
        }

        if (frame->hw_frames_ctx) {
            auto *device = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data)->device_ctx;
            if (device->type != reinterpret_cast<AVHWDeviceContext *>(d->hwDeviceContext->data)->type) {
                qWarning() << "Frame device type does not match encoder device type";
                return EINVAL;
            }
        }

        //        if (d->hwFrame) {
        //            av_frame_free(&d->hwFrame);
        //        }
        //        if (!frame->hw_frames_ctx || (frame->hw_frames_ctx->buffer != d->hwFramesContext->buffer)) {
        //            qDebug("[AVQt::VAAPIEncoderImpl1] Unprepared frame");
        //            ret = d->mapFrameToHW(&d->hwFrame, frame);
        //            if (ret < 0) {
        //                qWarning() << "Could not map frame to hw:" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
        //                return ret;
        //            }
        //        } else {
        d->hwFrame = frame;
        //        }
        //        qWarning("surface %ld refcount: %d", reinterpret_cast<intptr_t>(frame->buf[0]->data), av_buffer_get_ref_count(frame->buf[0]));

        if (ret < 0) {
            qWarning() << "Could not map frame to hw:" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
            return AVUNERROR(ret);
        }

        {
            QMutexLocker codecLocker(&d->codecMutex);
            //            QMutexLocker hwFramesLocker(hwFramesMutex);
            //            qDebug("[AVQt::VAAPIEncoderImpl2] Trying to lock hwframes context mutex %p", hwFramesMutex);
            //            hwFramesMutex->lock();
            //            qDebug("[AVQt::VAAPIEncoderImpl2] Using hwframes context mutex %p", hwFramesMutex);
            auto t1 = std::chrono::high_resolution_clock::now();
            // TODO: Fix this being too slow, exactly the send frame call
            ret = avcodec_send_frame(d->codecContext, d->hwFrame);
            auto t2 = std::chrono::high_resolution_clock::now();
            qDebug("[AVQt::VAAPIEncoderImpl2] avcodec_send_frame took %ld ns", std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count());
            //                        ret = 0;// Dummy
            //            hwFramesMutex->unlock();
        }
        d->hwFrame = nullptr;
        //        av_frame_free(&d->hwFrame);
        //        qDebug("[AVQt::VAAPIEncoderImpl2] Unlocked frame pool mutex: %p", hwFramesMutex);
        if (ret == AVERROR(EAGAIN)) {
            return EAGAIN;
        } else if (ret < 0) {
            //            av_frame_free(&d->hwFrame);
            qWarning() << "Could not send frame:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
            return AVUNERROR(ret);
        }

        d->firstFrame = false;

        return EXIT_SUCCESS;
    }

    AVPacket *VAAPIEncoderImpl::nextPacket() {
        Q_D(VAAPIEncoderImpl);
        //        if (!d->initialized) {
        //            qWarning() << "Encoder not initialized";
        //            return nullptr;
        //        }
        if (!d->packetFetcher) {
            return nullptr;
        }
        return d->packetFetcher->nextPacket();
    }

    QVector<AVPixelFormat> VAAPIEncoderImpl::getInputFormats() const {
        Q_D(const VAAPIEncoderImpl);
        if (d->firstFrame) {
            return QVector<AVPixelFormat>{AV_PIX_FMT_VAAPI, AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV420P10, AV_PIX_FMT_P010};
        } else {
            auto framesContext = reinterpret_cast<AVHWFramesContext *>(d->hwFramesContext->data);
            return QVector<AVPixelFormat>{framesContext->sw_format, framesContext->format};
        }
    }

    bool VAAPIEncoderImpl::isHWAccel() const {
        return true;
    }

    AVCodecParameters *VAAPIEncoderImpl::getCodecParameters() const {
        Q_D(const VAAPIEncoderImpl);
        AVCodecParameters *params = avcodec_parameters_alloc();
        avcodec_parameters_from_context(params, d->codecContext);
        params->format = reinterpret_cast<AVHWFramesContext *>(d->hwFramesContext->data)->sw_format;
        return params;
    }

    AVFrame *VAAPIEncoderImpl::prepareFrame(AVFrame *frame) {
        Q_D(VAAPIEncoderImpl);
        if (!d->initialized) {
            qWarning() << "Encoder not initialized";
            return nullptr;
        }
        if (!frame) {
            qWarning() << "No frame";
            return nullptr;
        }

        AVFrame *queueFrame = av_frame_alloc();

        switch (frame->format) {
                //            case AV_PIX_FMT_VAAPI:
            case AV_PIX_FMT_DRM_PRIME:
            case AV_PIX_FMT_CUDA:
            case AV_PIX_FMT_OPENCL:
            case AV_PIX_FMT_QSV:
                qDebug("Transferring frame from GPU to CPU");
                av_hwframe_transfer_data(queueFrame, frame, 0);
                queueFrame->pts = frame->pts;
                break;
            default:
                qDebug("Referencing frame");
                av_frame_ref(queueFrame, frame);
                break;
        }

        int ret;
        char strBuf[256];

        if (!d->hwFramesContext && queueFrame->hw_frames_ctx) {// If we don't have a hw context and the encoder is not open, we have to create a frames context first
            if (queueFrame->format == AV_PIX_FMT_VAAPI) {
                d->hwFramesContext = av_buffer_ref(queueFrame->hw_frames_ctx);
            } else {
                ret = av_hwframe_ctx_create_derived(&d->hwFramesContext, AV_PIX_FMT_VAAPI, d->hwDeviceContext, queueFrame->hw_frames_ctx, AV_HWFRAME_MAP_READ);
                if (ret < 0) {
                    qWarning() << "Could not create hw frame context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    goto fail;
                }
            }
            d->codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext);
        } else if (!d->hwFramesContext && !queueFrame->hw_frames_ctx) {
            qWarning() << "queueFrame does not have a hw context, but it is required by configured params";
            d->hwFramesContext = av_hwframe_ctx_alloc(d->hwDeviceContext);
            if (!d->hwFramesContext) {
                qWarning() << "Could not create hw frame context";
                goto fail;
            }
            d->codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext);
            auto framesContext = reinterpret_cast<AVHWFramesContext *>(d->hwFramesContext->data);
            framesContext->format = AV_PIX_FMT_VAAPI;
            framesContext->sw_format = static_cast<AVPixelFormat>(queueFrame->format);
            framesContext->width = queueFrame->width;
            framesContext->height = queueFrame->height;
            framesContext->initial_pool_size = 10;

            ret = av_hwframe_ctx_init(d->hwFramesContext);
            if (ret < 0) {
                qWarning() << "Could not initialize hw frame context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                goto fail;
            }
        }

        {
            d->codecMutex.lock();
            if (!avcodec_is_open(d->codecContext)) {
                ret = avcodec_open2(d->codecContext, d->codec, nullptr);
                if (ret < 0) {
                    qWarning() << "Could not open encoder:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    goto fail;
                }
            }
            d->codecMutex.unlock();
        }

        AVFrame *result;
        ret = d->mapFrameToHW(&result, queueFrame);
        if (ret != EXIT_SUCCESS) {
            qWarning() << "Could not map frame to hw:" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
            goto fail;
        }
        av_frame_free(&queueFrame);

        return result;

    fail:
        av_frame_free(&queueFrame);
        return nullptr;
    }

    int VAAPIEncoderImplPrivate::mapFrameToHW(AVFrame **output, AVFrame *frame) {
        int ret;
        char strBuf[256];

        AVFrame *result;

        if (frame->format == AV_PIX_FMT_VAAPI) {
            //            auto *hwFramesMutex = reinterpret_cast<HWContextSync *>(reinterpret_cast<AVHWFramesContext *>(hwFramesContext->data)->user_opaque);
            {
                //                QMutexLocker hwFramesLocker(hwFramesMutex);
                //                qDebug("[AVQt::VAAPIEncoderImpl::Fetcher] Trying to lock hwframes context mutex %p", hwFramesMutex);
                //                hwFramesMutex->lock();
                //                qDebug("[AVQt::VAAPIEncoderImpl::Fetcher] Using hwframes context mutex %p", hwFramesMutex);
                //                if (hwFrame) {
                //                    av_frame_free(&hwFrame);
                //                }
                result = av_frame_clone(frame);
                //                hwFramesMutex->unlock();
            }
            //            qDebug("[AVQt::VAAPIEncoderImpl::Fetcher] Unlocked frame pool mutex: %p", hwFramesMutex);
        } else if (frame->hw_frames_ctx && derivedContext) {
            //            if (hwFrame) {
            //                av_frame_unref(hwFrame);
            //            } else {
            result = av_frame_alloc();
            //            }
            result->format = AV_PIX_FMT_VAAPI;
            result->hw_frames_ctx = av_buffer_ref(frame->hw_frames_ctx);
            ret = av_hwframe_map(result, frame, AV_HWFRAME_MAP_READ);
            if (ret < 0) {
                qWarning() << "Could not map frame:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                av_frame_free(&result);
                return AVUNERROR(ret);
            }
        } else {
            if (frame->hw_frames_ctx && !derivedContext) {
                allocateHWFrame(&result);

                ret = av_frame_copy_props(result, frame);
                if (ret < 0) {
                    qWarning() << "Could not copy frame properties:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    av_frame_free(&result);
                    return AVUNERROR(ret);
                }
                AVFrame *swFrame = av_frame_alloc();

                // Copy frame from source to frames context (Copies twice, so it's not the fastest way)
                ret = av_hwframe_transfer_data(swFrame, frame, 0);
                if (ret < 0) {
                    qWarning() << "Could not transfer frame data:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    av_frame_free(&result);
                    av_frame_free(&swFrame);
                    return AVUNERROR(ret);
                }
                ret = av_hwframe_transfer_data(result, swFrame, 0);
                if (ret < 0) {
                    qWarning() << "Could not transfer frame data:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    av_frame_free(&result);
                    av_frame_free(&swFrame);
                    return AVUNERROR(ret);
                }

                av_frame_free(&swFrame);
            } else if (!frame->hw_frames_ctx) {
                ret = allocateHWFrame(&result);
                if (ret != EXIT_SUCCESS) {
                    qWarning() << "Could not allocate hw frame:" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
                    return ret;
                }
                ret = av_frame_copy_props(result, frame);
                if (ret < 0) {
                    qWarning() << "Could not copy frame properties:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    av_frame_free(&result);
                    return AVUNERROR(ret);
                }

                ret = av_hwframe_transfer_data(result, frame, 0);
                if (ret < 0) {
                    qWarning() << "Could not transfer frame data:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    av_frame_free(&result);
                    return AVUNERROR(ret);
                }
            } else {
                return EINVAL;
            }
        }

        *output = result;

        return EXIT_SUCCESS;
    }
    int VAAPIEncoderImplPrivate::allocateHWFrame(AVFrame **output) {
        int ret;
        char strBuf[256];
        *output = av_frame_alloc();
        if (!*output) {
            qWarning() << "Could not allocate hw frame";
            return ENOMEM;
        }

        ret = av_hwframe_get_buffer(hwFramesContext, *output, 0);
        if (ret < 0) {
            qWarning() << "Could not getFBO hw frame buffer:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
            av_frame_free(output);
            return AVUNERROR(ret);
        }
        return EXIT_SUCCESS;
    }

    internal::PacketFetcher::PacketFetcher(VAAPIEncoderImplPrivate *p)
        : QThread(), p(p) {
    }

    void internal::PacketFetcher::run() {
        qDebug() << "PacketFetcher started";
        int ret;
        constexpr size_t strBufSize = 256;
        char strBuf[strBufSize];

        AVPacket *packet = av_packet_alloc();
        if (!packet) {
            qWarning() << "Could not allocate packet";
            return;
        }
        while (!m_stop) {
            if (p->firstFrame) {
                msleep(10);
                continue;
            }
            QElapsedTimer timer;
            timer.start();

            {
                QMutexLocker codecLock(&p->codecMutex);
                ret = avcodec_receive_packet(p->codecContext, packet);
            }

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                msleep(1);
            } else if (ret < 0) {
                qWarning() << "Could not receive packet:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
            } else {
                //                QMutexLocker outputLock(&m_outputQueueMutex);
                //                m_outputQueue.push(packet);
                auto sharedPacket = std::shared_ptr<AVPacket>(av_packet_clone(packet), *p->packetDestructor);
                p->q_func()->packetReady(sharedPacket);
            }
            av_packet_unref(packet);
        }

        qDebug() << "PacketFetcher stopped";
    }

    void internal::PacketFetcher::stop() {
        if (isRunning()) {
            m_stop = true;
            QThread::quit();
            QThread::wait();
            {
                QMutexLocker locker(&m_outputQueueMutex);
                while (!m_outputQueue.empty()) {
                    AVPacket *packet = m_outputQueue.front();
                    m_outputQueue.pop();
                    av_packet_free(&packet);
                }
            }
        } else {
            qWarning() << "PacketFetcher not running";
        }
    }

    AVPacket *internal::PacketFetcher::nextPacket() {
        if (m_outputQueue.empty()) {
            return nullptr;
        }
        QMutexLocker locker(&m_outputQueueMutex);
        auto *packet = m_outputQueue.front();
        m_outputQueue.pop();
        return packet;
    }

    bool internal::PacketFetcher::isEmpty() const {
        return m_outputQueue.empty();
    }
}// namespace AVQt
