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
// Created by silas on 28.12.21.
//

#include "VAAPIEncoderImpl.hpp"
#include "encoder/Encoder.hpp"
#include "private/VAAPIEncoderImpl_p.hpp"
#include <QImage>

namespace AVQt {
    VAAPIEncoderImpl::VAAPIEncoderImpl(const EncodeParameters &parameters)
        : IEncoderImpl(parameters),
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
    }

    bool VAAPIEncoderImpl::open(const VideoPadParams &params) {
        Q_D(VAAPIEncoderImpl);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            int ret;
            char strBuf[256];

            d->codecContext = avcodec_alloc_context3(d->codec);
            if (!d->codecContext) {
                qWarning() << "Could not allocate codec context";
                goto fail;
            }

            d->codecContext->bit_rate = 10000000;
            d->codecContext->width = 1920;
            d->codecContext->height = 1080;
            d->codecContext->framerate = AVRational{60, 1};

            bool createContext = false;
            auto hwDeviceContext = reinterpret_cast<AVHWDeviceContext *>(params.hwDeviceContext->data);
            if (params.hwDeviceContext && hwDeviceContext->type == AV_HWDEVICE_TYPE_VAAPI) {
                createContext = false;
                d->hwDeviceContext = av_buffer_ref(params.hwDeviceContext);
                if (!d->hwDeviceContext) {
                    qWarning() << "Could not create hw device context";
                    createContext = true;
                }
            }
            if (createContext) {
                ret = av_hwdevice_ctx_create(&d->hwDeviceContext, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
                if (ret < 0) {
                    av_strerror(ret, strBuf, sizeof(strBuf));
                    qWarning() << "Failed to create VAAPI device context: " << strBuf;
                    goto fail;
                }

                d->hwFramesContext = av_hwframe_ctx_alloc(d->hwDeviceContext);
                if (!d->hwFramesContext) {
                    qWarning() << "Failed to create VAAPI frames context";
                    goto fail;
                }
                auto framesContext = reinterpret_cast<AVHWFramesContext *>(d->hwFramesContext->data);
                framesContext->format = AV_PIX_FMT_VAAPI;
                framesContext->sw_format = params.isHWAccel ? params.swPixelFormat : params.pixelFormat;
                framesContext->width = d->codecContext->width;
                framesContext->height = d->codecContext->height;
                framesContext->initial_pool_size = 20;
                ret = av_hwframe_ctx_init(d->hwFramesContext);
                if (ret < 0) {
                    av_strerror(ret, strBuf, sizeof(strBuf));
                    qWarning() << "Failed to initialize VAAPI frames context: " << strBuf;
                    goto fail;
                }
            }

            d->codecContext->pix_fmt = AV_PIX_FMT_VAAPI;
            d->codecContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext);
            if (d->hwFramesContext) {
                d->codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext);
            }
            d->codecContext->time_base = AVRational{1, 1000000};// microseconds
            d->codecContext->max_b_frames = 0;

            if (d->hwFramesContext) {
                ret = avcodec_open2(d->codecContext, d->codec, nullptr);
                if (ret < 0) {
                    av_strerror(ret, strBuf, sizeof(strBuf));
                    qWarning() << "Failed to open codec: " << strBuf;
                    goto fail;
                }

                d->hwFrame = av_frame_alloc();
                if (!d->hwFrame) {
                    qWarning() << "Failed to allocate frame";
                    goto fail;
                }
                if (av_hwframe_get_buffer(d->hwFramesContext, d->hwFrame, 0) < 0) {
                    qWarning() << "Failed to allocate frame buffer";
                    goto fail;
                }

                d->packetFetcher = new VAAPIEncoderImplPrivate::PacketFetcher(d);
                d->packetFetcher->start();
                if (!d->packetFetcher->isRunning()) {
                    qWarning() << "Failed to start packet fetcher";
                    goto fail;
                }
            }

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
        if (!d->codecContext) {
            qWarning() << "Codec context not initialized";
            return ENODEV;
        }

        int ret;
        char strBuf[256];

        if (!frame->hw_frames_ctx) {
            ret = d->allocateHwFrame();
            if (ret != EXIT_SUCCESS) {
                av_strerror(ret, strBuf, sizeof(strBuf));
                qWarning() << "Failed to allocate hw frame: " << strBuf;
                return ret;
            }
            ret = av_hwframe_transfer_data(d->hwFrame, frame, 0);
            if (ret < 0) {
                av_strerror(ret, strBuf, sizeof(strBuf));
                qWarning() << "Failed to transfer frame data: " << strBuf;
                return AVUNERROR(ret);
            }
        } else {
            if (!d->hwFramesContext) {
                //                if (frame->format == AV_PIX_FMT_VAAPI) {
                //                    d->hwFramesContext = av_buffer_ref(frame->hw_frames_ctx);
                //                    d->mappable = true;
                //                } else if (frame->format != AV_PIX_FMT_VAAPI) {
                d->mappable = true;
                ret = av_hwframe_ctx_create_derived(
                        &d->hwFramesContext,
                        AV_PIX_FMT_VAAPI,
                        d->hwDeviceContext,
                        frame->hw_frames_ctx,
                        AV_HWFRAME_MAP_READ);
                if (ret < 0) {
                    d->mappable = false;
                    qWarning() << "Could not create derived VAAPI frames context: " << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    d->hwFramesContext = av_hwframe_ctx_alloc(d->hwDeviceContext);
                    if (!d->hwFramesContext) {
                        qWarning() << "Failed to create VAAPI frames context";
                        return ENODEV;
                    }
                    auto framesContext = reinterpret_cast<AVHWFramesContext *>(d->hwFramesContext->data);
                    framesContext->format = AV_PIX_FMT_VAAPI;
                    framesContext->sw_format = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data)->sw_format;
                    framesContext->width = d->codecContext->width;
                    framesContext->height = d->codecContext->height;
                    framesContext->initial_pool_size = 20;
                    ret = av_hwframe_ctx_init(d->hwFramesContext);
                    if (ret < 0) {
                        av_strerror(ret, strBuf, sizeof(strBuf));
                        qWarning() << "Failed to initialize VAAPI frames context: " << strBuf;
                        return AVUNERROR(ret);
                    }
                }
                //                } else {
                //                    qFatal("Invalid frame format");
                //                }

                d->codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext);
                ret = avcodec_open2(d->codecContext, d->codec, nullptr);
                if (ret < 0) {
                    av_strerror(ret, strBuf, sizeof(strBuf));
                    qWarning() << "Failed to open codec: " << strBuf;
                    return AVUNERROR(ret);
                }

                d->packetFetcher = new VAAPIEncoderImplPrivate::PacketFetcher(d);
                d->packetFetcher->start();
                if (!d->packetFetcher->isRunning()) {
                    qWarning() << "Failed to start packet fetcher";
                    return ECHILD;
                }
            }
            ret = d->allocateHwFrame();
            if (ret != EXIT_SUCCESS) {
                qWarning() << "Failed to allocate frame buffer";
                av_frame_free(&d->hwFrame);
                return AVUNERROR(ret);
            }
            if (!d->mappable) {
                AVFrame *tmp = av_frame_alloc();
                ret = av_hwframe_transfer_data(tmp, frame, 0);
                if (ret < 0) {
                    av_strerror(ret, strBuf, sizeof(strBuf));
                    qWarning() << "Failed to transfer frame data: " << strBuf;
                    av_frame_free(&tmp);
                    return AVUNERROR(ret);
                }
                ret = av_hwframe_transfer_data(d->hwFrame, tmp, 0);
                if (ret < 0) {
                    av_strerror(ret, strBuf, sizeof(strBuf));
                    qWarning() << "Failed to transfer frame data: " << strBuf;
                    av_frame_free(&tmp);
                    av_frame_unref(d->hwFrame);
                    return AVUNERROR(ret);
                }
                av_frame_free(&tmp);
            } else if (d->hwFramesContext->buffer != frame->hw_frames_ctx->buffer) {
                ret = av_hwframe_map(d->hwFrame, frame, AV_HWFRAME_MAP_READ);
                if (ret < 0) {
                    av_strerror(ret, strBuf, sizeof(strBuf));
                    qWarning() << "Failed to map frame: " << strBuf;
                    return ret;
                }
            } else {
                av_frame_unref(d->hwFrame);
                ret = av_frame_ref(d->hwFrame, frame);
                if (ret < 0) {
                    av_strerror(ret, strBuf, sizeof(strBuf));
                    qWarning() << "Failed to copy frame: " << strBuf;
                    return ret;
                }
            }
        }

        d->hwFrame->pts = frame->pts;
        //        AVFrame *swFrame = av_frame_alloc();
        //        //        AVFrame *swFrame = av_frame_clone(tmp);
        //        av_hwframe_transfer_data(swFrame, d->hwFrame, 0);
        //        QImage image{swFrame->data[0], d->hwFrame->width, d->hwFrame->height, swFrame->linesize[0], QImage::Format_Grayscale8};
        //        image.save("output32.bmp");
        //        abort();
        //        exit(0);

        {
            qDebug("Frame %lu", d->frameCounter++);
            QMutexLocker locker(&d->codecMutex);
            ret = avcodec_send_frame(d->codecContext, d->hwFrame);
            locker.unlock();
            av_frame_free(&d->hwFrame);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN) || ret == AVERROR(ENOMEM)) {
                d->firstFrame = false;
                return EAGAIN;
            } else if (ret < 0) {
                qWarning("Frame#%lu Could not send frame to codec: %s", d->frameCounter.load(), av_make_error_string(strBuf, sizeof(strBuf), ret));
                return AVUNERROR(ret);
            }
        }

        d->firstFrame = false;

        return EXIT_SUCCESS;
    }

    AVPacket *VAAPIEncoderImpl::nextPacket() {
        Q_D(VAAPIEncoderImpl);
        if (!d->initialized) {
            qWarning() << "Encoder not initialized";
            return nullptr;
        }
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

    int VAAPIEncoderImplPrivate::allocateHwFrame() {
        int ret;
        if (hwFrame) {
            av_frame_free(&hwFrame);
        }
        if (!hwFramesContext) {
            qWarning() << "No hardware frames context";
            return AVERROR(EINVAL);
        }
        hwFrame = av_frame_alloc();
        if (!hwFrame) {
            qWarning() << "Failed to allocate frame";
            return ENOMEM;
        }
        if (!mappable) {
            if ((ret = av_hwframe_get_buffer(hwFramesContext, hwFrame, 0)) < 0) {
                qWarning() << "Failed to allocate frame buffer";
                return AVUNERROR(ret);
            }
        } else {
            //            hwFrame->format = AV_PIX_FMT_VAAPI;
            hwFrame->hw_frames_ctx = av_buffer_ref(hwFramesContext);
        }
        return EXIT_SUCCESS;
    }

    VAAPIEncoderImplPrivate::PacketFetcher::PacketFetcher(VAAPIEncoderImplPrivate *p)
        : p(p) {
    }

    void VAAPIEncoderImplPrivate::PacketFetcher::run() {
        qDebug() << "PacketFetcher started";
        int ret;
        constexpr size_t strBufSize = 256;
        char strBuf[strBufSize];
        while (!m_stop) {
            QMutexLocker codecLock(&p->codecMutex);
            if (p->firstFrame) {
                codecLock.unlock();
                msleep(2);
                continue;
            }
            AVPacket *packet = av_packet_alloc();
            ret = avcodec_receive_packet(p->codecContext, packet);
            codecLock.unlock();
            if (ret == 0) {
                QMutexLocker outputLock(&m_mutex);
                qDebug("Enqueuing packet with PTS %ld", packet->pts);
                m_outputQueue.enqueue(packet);
            } else if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                av_packet_free(&packet);
                msleep(1);
            } else {
                av_packet_free(&packet);
                av_strerror(ret, strBuf, strBufSize);
                qWarning("Could not receive packet: %s", strBuf);
                m_stop = true;
                break;
            }
        }
        qDebug() << "PacketFetcher stopped";
    }

    void VAAPIEncoderImplPrivate::PacketFetcher::stop() {
        if (isRunning()) {
            m_stop = true;
            QThread::quit();
            QThread::wait();
            {
                QMutexLocker locker(&m_mutex);
                while (!m_outputQueue.isEmpty()) {
                    AVPacket *packet = m_outputQueue.dequeue();
                    av_packet_free(&packet);
                }
            }
        } else {
            qWarning() << "PacketFetcher not running";
        }
    }

    AVPacket *VAAPIEncoderImplPrivate::PacketFetcher::nextPacket() {
        QMutexLocker locker(&m_mutex);
        if (m_outputQueue.isEmpty()) {
            return nullptr;
        }
        return m_outputQueue.dequeue();
    }

    bool VAAPIEncoderImplPrivate::PacketFetcher::isEmpty() const {
        return m_outputQueue.isEmpty();
    }
}// namespace AVQt
