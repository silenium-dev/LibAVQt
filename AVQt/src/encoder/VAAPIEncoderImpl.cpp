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
#include "AVQt/encoder/VideoEncoder.hpp"
#include "private/VAAPIEncoderImpl_p.hpp"

#include <QImage>
#include <QtConcurrent>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace AVQt {
    const QList<AVPixelFormat> VAAPIEncoderImplPrivate::supportedPixelFormats{
            AV_PIX_FMT_NV12,
            AV_PIX_FMT_P010,
            AV_PIX_FMT_YUV420P,
            AV_PIX_FMT_YUV420P10,
            AV_PIX_FMT_VAAPI};
    const api::VideoEncoderInfo VAAPIEncoderImpl::info{
            .metaObject = VAAPIEncoderImpl::staticMetaObject,
            .name = "VAAPI",
            .platforms = {
                    common::Platform::Linux_Wayland,
                    common::Platform::Linux_X11,
            },
            .supportedInputPixelFormats = {
                    {AV_PIX_FMT_NV12, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_P010, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_NV12, AV_PIX_FMT_VAAPI_VLD},
                    {AV_PIX_FMT_P010, AV_PIX_FMT_VAAPI_VLD},
                    {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_YUV420P10, AV_PIX_FMT_NONE},
                    {AV_PIX_FMT_NONE, AV_PIX_FMT_VAAPI},
            },
            .supportedCodecIds = {
                    AV_CODEC_ID_H264,
                    AV_CODEC_ID_HEVC,
                    AV_CODEC_ID_VP8,
                    AV_CODEC_ID_VP9,
                    AV_CODEC_ID_MPEG2VIDEO,
            },
    };

    VAAPIEncoderImpl::VAAPIEncoderImpl(AVCodecID codec, EncodeParameters parameters)
        : QObject(),
          IVideoEncoderImpl(parameters),
          d_ptr(new VAAPIEncoderImplPrivate(this)) {
        Q_D(VAAPIEncoderImpl);

        QString codecName;
        switch (codec) {
            case AV_CODEC_ID_H264:
                codecName = "h264_vaapi";
                break;
            case AV_CODEC_ID_HEVC:
                codecName = "hevc_vaapi";
                break;
            case AV_CODEC_ID_VP8:
                codecName = "vp8_vaapi";
                break;
            case AV_CODEC_ID_VP9:
                codecName = "vp9_vaapi";
                break;
            case AV_CODEC_ID_MPEG2VIDEO:
                codecName = "mpeg2_vaapi";
                break;
            default:
                qWarning() << "Unsupported codec" << avcodec_get_name(codec);
                return;
        }

        d->codec = avcodec_find_encoder_by_name(qPrintable(codecName));
        if (!d->codec) {
            qWarning() << "Codec not found";
            return;
        }
        d->encodeParameters = parameters;
    }

    VAAPIEncoderImpl::~VAAPIEncoderImpl() {
        Q_D(VAAPIEncoderImpl);
        if (d->initialized) {
            VAAPIEncoderImpl::close();
        }
    }

    bool VAAPIEncoderImpl::open(const communication::VideoPadParams &params) {
        Q_D(VAAPIEncoderImpl);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            int ret;
            char strBuf[256];
            bool createContext = false;

            d->codecContext = {avcodec_alloc_context3(d->codec), &VAAPIEncoderImplPrivate::destroyAVCodecContext};
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
            } else if (params.hwDeviceContext /* && params.pixelFormat == AV_PIX_FMT_VAAPI (implicit) */) {
                createContext = false;
                d->derivedContext = false;
                d->hwDeviceContext = params.hwDeviceContext;
                d->hwFramesContext = params.hwFramesContext;
            }

            if (createContext) {
                if (d->derivedContext) {
                    AVBufferRef *hwDeviceContext;
                    ret = av_hwdevice_ctx_create_derived(&hwDeviceContext, AV_HWDEVICE_TYPE_VAAPI, params.hwDeviceContext.get(), 0);
                    if (ret < 0) {
                        qWarning() << "Could not create derived device context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                        goto fail;
                    }
                    d->hwDeviceContext = {hwDeviceContext, &VAAPIEncoderImplPrivate::destroyAVBufferRef};
                } else {
                    AVBufferRef *hwDeviceContext;
                    ret = av_hwdevice_ctx_create(&hwDeviceContext, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
                    if (ret < 0) {
                        qWarning() << "Could not create device context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                        goto fail;
                    }
                    d->hwDeviceContext = {hwDeviceContext, &VAAPIEncoderImplPrivate::destroyAVBufferRef};

                    d->hwFramesContext = {av_hwframe_ctx_alloc(d->hwDeviceContext.get()), &VAAPIEncoderImplPrivate::destroyAVBufferRef};
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

                    ret = av_hwframe_ctx_init(d->hwFramesContext.get());
                    if (ret < 0) {
                        qWarning() << "Could not initialize hw frame context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                        d->hwFramesContext.reset();
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
            d->codecContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext.get());
            d->codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext.get());

            ret = avcodec_open2(d->codecContext.get(), d->codec, nullptr);
            if (ret < 0) {
                qWarning() << "Could not open codec:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                goto fail;
            }

            d->packetFetcher = std::make_unique<internal::PacketFetcher>(d);
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
                d->packetFetcher.reset();
            }
            d->hwFrame.reset();
            d->codecContext.reset();
            d->codecParams.reset();
            d->hwDeviceContext.reset();
            d->hwFramesContext.reset();
        }
    }

    int VAAPIEncoderImpl::encode(std::shared_ptr<AVFrame> frame) {
        Q_D(VAAPIEncoderImpl);

        int ret = 0;
        char strBuf[256];

        if (!d->codecContext) {
            qWarning() << "Codec context not allocated";
            return ENODEV;
        }

        if (!frame) {
            qWarning() << "Frame is empty";
            return ENODATA;
        }

        if (frame->hw_frames_ctx) {
            auto *device = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data)->device_ctx;
            if (device->type != reinterpret_cast<AVHWDeviceContext *>(d->hwDeviceContext->data)->type) {
                qWarning() << "Frame device type does not match encoder device type";
                return EINVAL;
            }
        }

        d->hwFrame = frame;

        if (ret < 0) {
            qWarning() << "Could not map frame to hw:" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
            return AVUNERROR(ret);
        }

        {
            QMutexLocker codecLocker(&d->codecMutex);
            auto t1 = std::chrono::high_resolution_clock::now();
            ret = avcodec_send_frame(d->codecContext.get(), d->hwFrame.get());
            auto t2 = std::chrono::high_resolution_clock::now();
            qDebug("[AVQt::VAAPIEncoderImpl2] avcodec_send_frame took %lld ns", std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count());
        }
        d->hwFrame.reset();
        if (ret == AVERROR(EAGAIN)) {
            return EAGAIN;
        } else if (ret < 0) {
            qWarning() << "Could not send frame:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
            return AVUNERROR(ret);
        }

        d->firstFrame = false;

        return EXIT_SUCCESS;
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

    std::shared_ptr<AVCodecParameters> VAAPIEncoderImpl::getCodecParameters() const {
        Q_D(const VAAPIEncoderImpl);
        std::shared_ptr<AVCodecParameters> params{avcodec_parameters_alloc(), &VAAPIEncoderImplPrivate::destroyAVCodecParameters};
        avcodec_parameters_from_context(params.get(), d->codecContext.get());
        params->format = reinterpret_cast<AVHWFramesContext *>(d->hwFramesContext->data)->sw_format;
        return params;
    }

    std::shared_ptr<AVFrame> VAAPIEncoderImpl::prepareFrame(std::shared_ptr<AVFrame> frame) {
        Q_D(VAAPIEncoderImpl);
        if (!d->initialized) {
            qWarning() << "VideoEncoder not initialized";
            return nullptr;
        }
        if (!frame) {
            qWarning() << "No frame";
            return nullptr;
        }

        std::shared_ptr<AVFrame> queueFrame{};

        switch (frame->format) {
            case AV_PIX_FMT_DRM_PRIME:
            case AV_PIX_FMT_CUDA:
            case AV_PIX_FMT_OPENCL:
            case AV_PIX_FMT_QSV:
                qDebug("Transferring frame from GPU to CPU");
                queueFrame = {av_frame_alloc(), &VAAPIEncoderImplPrivate::destroyAVFrame};
                av_hwframe_transfer_data(queueFrame.get(), frame.get(), 0);
                queueFrame->pts = frame->pts;
                break;
            default:
                qDebug("Referencing frame");
                queueFrame = frame;
                break;
        }

        int ret;
        char strBuf[256];

        if (!d->hwFramesContext && queueFrame->hw_frames_ctx) {// If we don't have a hw context and the encoder is not open, we have to create a frames context first
            if (queueFrame->format == AV_PIX_FMT_VAAPI) {
                d->hwFramesContext = {av_buffer_ref(queueFrame->hw_frames_ctx), &VAAPIEncoderImplPrivate::destroyAVBufferRef};
            } else {
                AVBufferRef *hwFramesContext;
                ret = av_hwframe_ctx_create_derived(&hwFramesContext, AV_PIX_FMT_VAAPI, d->hwDeviceContext.get(), queueFrame->hw_frames_ctx, AV_HWFRAME_MAP_READ);
                if (ret < 0) {
                    qWarning() << "Could not create hw frame context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    goto fail;
                }
                d->hwFramesContext = {hwFramesContext, &VAAPIEncoderImplPrivate::destroyAVBufferRef};
            }
            d->codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext.get());
        } else if (!d->hwFramesContext && !queueFrame->hw_frames_ctx) {
            qWarning() << "queueFrame does not have a hw context, but it is required by configured params";
            auto hwFramesContext = av_hwframe_ctx_alloc(d->hwDeviceContext.get());
            if (!hwFramesContext) {
                qWarning() << "Could not create hw frame context";
                goto fail;
            }
            auto framesContext = reinterpret_cast<AVHWFramesContext *>(d->hwFramesContext->data);
            framesContext->format = AV_PIX_FMT_VAAPI;
            framesContext->sw_format = static_cast<AVPixelFormat>(queueFrame->format);
            framesContext->width = queueFrame->width;
            framesContext->height = queueFrame->height;
            framesContext->initial_pool_size = 10;

            ret = av_hwframe_ctx_init(hwFramesContext);
            if (ret < 0) {
                qWarning() << "Could not initialize hw frame context:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                goto fail;
            }
            d->hwFramesContext = {hwFramesContext, VAAPIEncoderImplPrivate::destroyAVBufferRef};

            d->codecContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext.get());
        }

        {
            d->codecMutex.lock();
            if (!avcodec_is_open(d->codecContext.get())) {
                ret = avcodec_open2(d->codecContext.get(), d->codec, nullptr);
                if (ret < 0) {
                    qWarning() << "Could not open encoder:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    goto fail;
                }
            }
            d->codecMutex.unlock();
        }

        {
            std::shared_ptr<AVFrame> result{};
            ret = d->mapFrameToHW(result, queueFrame);
            if (ret != EXIT_SUCCESS) {
                qWarning() << "Could not map frame to hw:" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
                goto fail;
            }

            return result;
        }
    fail:
        return {};
    }

    int VAAPIEncoderImplPrivate::mapFrameToHW(std::shared_ptr<AVFrame> &output, const std::shared_ptr<AVFrame> &frame) {
        int ret;
        char strBuf[256];

        std::shared_ptr<AVFrame> result{};

        if (frame->format == AV_PIX_FMT_VAAPI) {
            result = frame;
        } else if (frame->hw_frames_ctx && derivedContext) {
            result = {av_frame_alloc(), &destroyAVFrame};
            result->format = AV_PIX_FMT_VAAPI;
            result->hw_frames_ctx = av_buffer_ref(frame->hw_frames_ctx);
            ret = av_hwframe_map(result.get(), frame.get(), AV_HWFRAME_MAP_READ);
            if (ret < 0) {
                qWarning() << "Could not map frame:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                result.reset();
                return AVUNERROR(ret);
            }
        } else {
            if (frame->hw_frames_ctx && !derivedContext) {
                ret = allocateHWFrame(result);
                if (ret != EXIT_SUCCESS) {
                    qWarning() << "Could not allocate hw frame:" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
                    return AVUNERROR(ret);
                }

                ret = av_frame_copy_props(result.get(), frame.get());
                if (ret < 0) {
                    qWarning() << "Could not copy frame properties:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    result.reset();
                    return AVUNERROR(ret);
                }
                std::shared_ptr<AVFrame> swFrame = {av_frame_alloc(), &destroyAVFrame};

                // Copy frame from source to frames context (Copies twice, so it's not the fastest way)
                ret = av_hwframe_transfer_data(swFrame.get(), frame.get(), 0);
                if (ret < 0) {
                    qWarning() << "Could not transfer frame data:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    result.reset();
                    return AVUNERROR(ret);
                }
                ret = av_hwframe_transfer_data(result.get(), swFrame.get(), 0);
                if (ret < 0) {
                    qWarning() << "Could not transfer frame data:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    result.reset();
                    return AVUNERROR(ret);
                }
            } else if (!frame->hw_frames_ctx) {
                ret = allocateHWFrame(result);
                if (ret != EXIT_SUCCESS) {
                    qWarning() << "Could not allocate hw frame:" << av_make_error_string(strBuf, sizeof(strBuf), AVERROR(ret));
                    return ret;
                }
                ret = av_frame_copy_props(result.get(), frame.get());
                if (ret < 0) {
                    qWarning() << "Could not copy frame properties:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    result.reset();
                    return AVUNERROR(ret);
                }

                ret = av_hwframe_transfer_data(result.get(), frame.get(), 0);
                if (ret < 0) {
                    qWarning() << "Could not transfer frame data:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    result.reset();
                    return AVUNERROR(ret);
                }
            } else {
                return EINVAL;
            }
        }

        output = std::move(result);

        return EXIT_SUCCESS;
    }
    int VAAPIEncoderImplPrivate::allocateHWFrame(std::shared_ptr<AVFrame> &output) {
        int ret;
        char strBuf[256];
        output = {av_frame_alloc(), &destroyAVFrame};
        if (!output) {
            qWarning() << "Could not allocate hw frame";
            return ENOMEM;
        }

        ret = av_hwframe_get_buffer(hwFramesContext.get(), output.get(), 0);
        if (ret < 0) {
            qWarning() << "Could not getFBO hw frame buffer:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
            output.reset();
            return AVUNERROR(ret);
        }
        return EXIT_SUCCESS;
    }

    void VAAPIEncoderImplPrivate::destroyAVBufferRef(AVBufferRef *buffer) {
        if (buffer) {
            av_buffer_unref(&buffer);
        }
    }

    void VAAPIEncoderImplPrivate::destroyAVCodecContext(AVCodecContext *codecContext) {
        if (codecContext) {
            if (avcodec_is_open(codecContext)) {
                avcodec_close(codecContext);
            }
            avcodec_free_context(&codecContext);
        }
    }

    void VAAPIEncoderImplPrivate::destroyAVCodecParameters(AVCodecParameters *codecParameters) {
        if (codecParameters) {
            avcodec_parameters_free(&codecParameters);
        }
    }

    void VAAPIEncoderImplPrivate::destroyAVFrame(AVFrame *frame) {
        if (frame) {
            av_frame_free(&frame);
        }
    }

    internal::PacketFetcher::PacketFetcher(VAAPIEncoderImplPrivate *p)
        : QThread(), p(p) {
    }

    void internal::PacketFetcher::run() {
        qDebug() << "PacketFetcher started";
        int ret;
        constexpr size_t strBufSize = 256;
        char strBuf[strBufSize];

        while (!m_stop) {
            if (p->firstFrame) {
                msleep(10);
                continue;
            }
            QElapsedTimer timer;
            timer.start();

            std::shared_ptr<AVPacket> packet{av_packet_alloc(), [](AVPacket *pak) {
                                                 av_packet_free(&pak);
                                             }};
            if (!packet) {
                qWarning() << "Could not allocate packet";
                return;
            }

            {
                QMutexLocker codecLock(&p->codecMutex);
                ret = avcodec_receive_packet(p->codecContext.get(), packet.get());
            }

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                msleep(1);
            } else if (ret < 0) {
                qWarning() << "Could not receive packet:" << av_make_error_string(strBuf, sizeof(strBuf), ret);
            } else {
                p->q_func()->packetReady(packet);
            }
        }

        qDebug() << "PacketFetcher stopped";
    }

    void internal::PacketFetcher::stop() {
        if (isRunning()) {
            m_stop = true;
            QThread::quit();
            QThread::wait();
        } else {
            qWarning() << "PacketFetcher not running";
        }
    }
}// namespace AVQt
