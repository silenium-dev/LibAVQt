// Copyright (c) 2022.
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
// Created by silas on 13.01.22.
//

#include "VaapiTranscoderImpl.hpp"
#include "private/VaapiTranscoderImpl_p.hpp"

#include <QtDebug>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace AVQt {
    VAAPITranscoderImpl::VAAPITranscoderImpl(EncodeParameters parameters, QObject *parent) : QObject(parent), d_ptr(new VAAPITranscoderImplPrivate(this)) {
        Q_D(VAAPITranscoderImpl);
        d->encodeParameters = parameters;
        d->encoder = VAAPITranscoderImplPrivate::getEncoder(d->encodeParameters.codec);
        d->frameDestructor = std::make_shared<internal::FrameDestructor>();
    }

    VAAPITranscoderImpl::~VAAPITranscoderImpl() {
        VAAPITranscoderImpl::close();
        delete d_ptr;
    }

    bool VAAPITranscoderImpl::open(const PacketPadParams &params) {
        Q_D(VAAPITranscoderImpl);

        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            int ret;
            char strBuf[256];
            d->decoder = VAAPITranscoderImplPrivate::getDecoder(params.codec);
            d->inputParams = params;

            {
                ret = av_hwdevice_ctx_create(&d->hwDeviceContext, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
                if (ret < 0) {
                    qWarning() << "Failed to create VAAPI device context: " << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    goto fail;
                }
            }
            {
                d->hwFramesContext = av_hwframe_ctx_alloc(d->hwDeviceContext);
                if (!d->hwFramesContext) {
                    qWarning() << "Failed to allocate VAAPI frames context";
                    return AV_PIX_FMT_NONE;
                }

                auto *framesContext = reinterpret_cast<AVHWFramesContext *>(d->hwFramesContext->data);
                framesContext->format = AV_PIX_FMT_VAAPI;
                framesContext->sw_format = VAAPITranscoderImplPrivate::getNativePixelFormat(d->inputParams.codecParams->format);
                framesContext->width = d->inputParams.codecParams->width;
                framesContext->height = d->inputParams.codecParams->height;
                framesContext->initial_pool_size = 32;

                if (av_hwframe_ctx_init(d->hwFramesContext) < 0) {
                    qWarning() << "Failed to initialize VAAPI frames context";
                    av_buffer_unref(&d->hwFramesContext);
                    return AV_PIX_FMT_NONE;
                }
            }
            {
                d->decodeContext = avcodec_alloc_context3(d->decoder);
                if (!d->decodeContext) {
                    qWarning() << "Could not allocate video codec context";
                    goto fail;
                }

                ret = avcodec_parameters_to_context(d->decodeContext, params.codecParams);
                if (ret < 0) {
                    qWarning() << "Failed to copy codec parameters to decoder context: " << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    goto fail;
                }

                d->decodeContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext);
                d->decodeContext->opaque = d;
                d->decodeContext->get_format = VAAPITranscoderImplPrivate::getFormat;

                ret = avcodec_open2(d->decodeContext, d->decoder, nullptr);
                if (ret < 0) {
                    qWarning() << "Failed to open codec: " << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    goto fail;
                }
            }
            {

                d->encodeContext = avcodec_alloc_context3(d->encoder);
                if (!d->encodeContext) {
                    qWarning() << "Could not allocate video codec context";
                    goto fail;
                }

                d->encodeContext->pix_fmt = AV_PIX_FMT_VAAPI;
                d->encodeContext->sw_pix_fmt = VAAPITranscoderImplPrivate::getNativePixelFormat(params.codecParams->format);
                d->encodeContext->hw_device_ctx = av_buffer_ref(d->hwDeviceContext);
                d->encodeContext->hw_frames_ctx = av_buffer_ref(d->hwFramesContext);
                d->encodeContext->width = params.codecParams->width;
                d->encodeContext->height = params.codecParams->height;
                d->encodeContext->time_base = av_make_q(1, 1000000);
                d->encodeContext->opaque = d;
                d->encodeContext->max_b_frames = 0;
                d->encodeContext->gop_size = 10;

                ret = avcodec_open2(d->encodeContext, d->encoder, nullptr);
                if (ret < 0) {
                    qWarning() << "Failed to open codec: " << av_make_error_string(strBuf, sizeof(strBuf), ret);
                    goto fail;
                }
            }

            {
                d->packetFetcher = std::make_unique<VAAPITranscoderImplPrivate::PacketFetcher>(d);
                d->packetFetcher->start();
            }

            return true;
        } else {
            qWarning() << "VAAPITranscoderImpl already initialized";
            return false;
        }
    fail:
        close();
        return false;
    }

    void VAAPITranscoderImpl::close() {
        Q_D(VAAPITranscoderImpl);

        bool shouldBe = true;
        if (d->initialized.compare_exchange_strong(shouldBe, false)) {
            if (d->decodeContext) {
                if (avcodec_is_open(d->decodeContext)) {
                    avcodec_close(d->decodeContext);
                }
                avcodec_free_context(&d->decodeContext);
            }
            if (d->encodeContext) {
                if (avcodec_is_open(d->encodeContext)) {
                    avcodec_close(d->encodeContext);
                }
                avcodec_free_context(&d->encodeContext);
            }
            if (d->hwDeviceContext) {
                av_buffer_unref(&d->hwDeviceContext);
            }
            if (d->hwFramesContext) {
                av_buffer_unref(&d->hwFramesContext);
            }
            if (d->packetFetcher) {
                d->packetFetcher->requestInterruption();
                d->packetFetcher->wait();
                d->packetFetcher.reset();
            }
        }
    }

    int VAAPITranscoderImpl::transcode(AVPacket *packet) {
        Q_D(VAAPITranscoderImpl);
        int result = EXIT_SUCCESS;

        if (!d->initialized) {
            qWarning() << "VAAPITranscoderImpl not initialized";
            return ENODEV;
        }

        if (!packet) {
            qWarning() << "Invalid packet";
            return EINVAL;
        }

        if (packet->size == 0) {
            qWarning() << "Empty packet";
            return EINVAL;
        }

        int ret;
        char strBuf[256];
        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            qWarning() << "Could not allocate video frame";
            return ENOMEM;
        }

        ret = avcodec_send_packet(d->decodeContext, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            goto fail;
        } else if (ret < 0) {
            qWarning() << "Failed to send packet to decoder: " << av_make_error_string(strBuf, sizeof(strBuf), ret);
            goto fail;
        }

        while (true) {
            int decret = avcodec_receive_frame(d->decodeContext, frame);
            if (decret == AVERROR(EAGAIN) || decret == AVERROR_EOF) {
                goto fail;
            } else if (ret < 0) {
                qWarning() << "Failed to receive frame from decoder: " << av_make_error_string(strBuf, sizeof(strBuf), ret);
                goto fail;
            }
            auto sharedFrame = std::shared_ptr<AVFrame>(frame, *d->frameDestructor);
            frame = nullptr;

            emit frameReady(sharedFrame);

            {
                QMutexLocker encoderLock(&d->encoderMutex);
                ret = avcodec_send_frame(d->encodeContext, frame);
            }
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                goto fail;
            } else if (ret < 0) {
                qWarning() << "Failed to send frame to encoder: " << av_make_error_string(strBuf, sizeof(strBuf), ret);
                goto fail;
            }
            frame = av_frame_alloc();
        }

    fail:
        if (frame) {
            av_frame_free(&frame);
        }
        return AVUNERROR(ret);
    }

    AVPacket *VAAPITranscoderImpl::nextPacket() {
        Q_D(VAAPITranscoderImpl);
        if (d->outputQueue.isEmpty()) {
            return nullptr;
        }
        QMutexLocker locker(&d->outputQueueMutex);
        return d->outputQueue.dequeue();
    }

    PacketPadParams VAAPITranscoderImpl::getPacketOutputParams() const {
        Q_D(const VAAPITranscoderImpl);
        AVCodecParameters *codecParams = avcodec_parameters_alloc();
        avcodec_parameters_from_context(codecParams, d->encodeContext);
        PacketPadParams result{};
        result.codecParams = codecParams;
        result.mediaType = AVMEDIA_TYPE_VIDEO;
        result.codec = d->encodeContext->codec_id;
        result.streamIdx = 0;
        return result;
    }

    PacketPadParams VAAPITranscoderImpl::getPacketOutputParamsForInputParams(PacketPadParams inputParams) const {
        Q_D(const VAAPITranscoderImpl);
        AVCodecParameters *codecParams = avcodec_parameters_alloc();
        avcodec_parameters_copy(codecParams, inputParams.codecParams);
        PacketPadParams result{};
        codecParams->codec_id = getCodecId(d->encodeParameters.codec);
        codecParams->bit_rate = d->encodeParameters.bitrate;
        result.codecParams = codecParams;
        result.mediaType = AVMEDIA_TYPE_VIDEO;
        result.codec = getCodecId(d->encodeParameters.codec);
        result.streamIdx = 0;
        return result;
    }

    VideoPadParams VAAPITranscoderImpl::getVideoOutputParams() const {
        Q_D(const VAAPITranscoderImpl);
        VideoPadParams result{};
        if (d->hwDeviceContext) {
            result.hwDeviceContext = av_buffer_ref(d->hwDeviceContext);
        }
        result.frameSize = {d->encodeContext->width, d->encodeContext->height};
        result.pixelFormat = d->encodeContext->pix_fmt;
        result.swPixelFormat = d->encodeContext->sw_pix_fmt;
        result.isHWAccel = true;

        return result;
    }

    VideoPadParams VAAPITranscoderImpl::getVideoOutputParamsForInputParams(PacketPadParams inputParams) const {
        Q_D(const VAAPITranscoderImpl);
        VideoPadParams result{};
        if (d->hwDeviceContext) {
            result.hwDeviceContext = av_buffer_ref(d->hwDeviceContext);
        }
        result.frameSize = {inputParams.codecParams->width, inputParams.codecParams->height};
        result.pixelFormat = AV_PIX_FMT_VAAPI;
        result.swPixelFormat = VAAPITranscoderImplPrivate::getNativePixelFormat(inputParams.codecParams->format);
        result.isHWAccel = true;

        return result;
    }

    AVCodec *VAAPITranscoderImplPrivate::getEncoder(Codec codec) {
        switch (codec) {
            case Codec::H264:
                return avcodec_find_encoder_by_name("h264_vaapi");
            case Codec::HEVC:
                return avcodec_find_encoder_by_name("hevc_vaapi");
            case Codec::VP8:
                return avcodec_find_encoder_by_name("vp8_vaapi");
            case Codec::VP9:
                return avcodec_find_encoder_by_name("vp9_vaapi");
            case Codec::MPEG2:
                return avcodec_find_encoder_by_name("mpeg2_vaapi");
        }
    }

    AVCodec *VAAPITranscoderImplPrivate::getDecoder(Codec codec) {
        switch (codec) {
            case Codec::H264:
                return avcodec_find_decoder(AV_CODEC_ID_H264);
            case Codec::HEVC:
                return avcodec_find_decoder(AV_CODEC_ID_HEVC);
            case Codec::VP8:
                return avcodec_find_decoder(AV_CODEC_ID_VP8);
            case Codec::VP9:
                return avcodec_find_decoder(AV_CODEC_ID_VP9);
            case Codec::MPEG2:
                return avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
        }
    }

    AVCodec *VAAPITranscoderImplPrivate::getEncoder(AVCodecID codec) {
        switch (codec) {
            case AV_CODEC_ID_H264:
                return avcodec_find_encoder_by_name("h264_vaapi");
            case AV_CODEC_ID_HEVC:
                return avcodec_find_encoder_by_name("hevc_vaapi");
            case AV_CODEC_ID_VP8:
                return avcodec_find_encoder_by_name("vp8_vaapi");
            case AV_CODEC_ID_VP9:
                return avcodec_find_encoder_by_name("vp9_vaapi");
            case AV_CODEC_ID_MPEG2VIDEO:
                return avcodec_find_encoder_by_name("mpeg2_vaapi");
            default:
                qWarning() << "Unsupported codec:" << avcodec_get_name(codec);
                return nullptr;
        }
    }

    AVCodec *VAAPITranscoderImplPrivate::getDecoder(AVCodecID codec) {
        switch (codec) {
            case AV_CODEC_ID_H264:
                return avcodec_find_decoder(AV_CODEC_ID_H264);
            case AV_CODEC_ID_HEVC:
                return avcodec_find_decoder(AV_CODEC_ID_HEVC);
            case AV_CODEC_ID_VP8:
                return avcodec_find_decoder(AV_CODEC_ID_VP8);
            case AV_CODEC_ID_VP9:
                return avcodec_find_decoder(AV_CODEC_ID_VP9);
            case AV_CODEC_ID_MPEG2VIDEO:
                return avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
            default:
                qWarning() << "Unsupported codec:" << avcodec_get_name(codec);
                return nullptr;
        }
    }

    AVPixelFormat VAAPITranscoderImplPrivate::getFormat(AVCodecContext *ctx, const AVPixelFormat *pixelFormats) {
        const AVPixelFormat *p = pixelFormats;
        AVPixelFormat result = AV_PIX_FMT_NONE;
        while (*p != AV_PIX_FMT_NONE) {
            if (*p == AV_PIX_FMT_VAAPI) {
                result = *p;
            }
            p++;
        }
        if (result == AV_PIX_FMT_NONE) {
            return AV_PIX_FMT_NONE;
        }

        auto *d = reinterpret_cast<VAAPITranscoderImplPrivate *>(ctx->opaque);

        if (!d->hwFramesContext) {
            qWarning() << "No hw frames context";
            return AV_PIX_FMT_NONE;
        }

        ctx->hw_frames_ctx = av_buffer_ref(d->hwFramesContext);

        return AV_PIX_FMT_VAAPI;
    }

    AVPixelFormat VAAPITranscoderImplPrivate::getNativePixelFormat(AVPixelFormat format) {
        switch (format) {
            case AV_PIX_FMT_YUV420P:
            case AV_PIX_FMT_NV12:
                return AV_PIX_FMT_NV12;
            case AV_PIX_FMT_YUV420P10:
            case AV_PIX_FMT_P010:
                return AV_PIX_FMT_P010;
            case AV_PIX_FMT_YUV420P16:
            case AV_PIX_FMT_P016:
                return AV_PIX_FMT_P016;
            default:
                qWarning() << "Unsupported pixel format:" << av_get_pix_fmt_name(format);
                return AV_PIX_FMT_NONE;
        }
    }

    AVPixelFormat VAAPITranscoderImplPrivate::getNativePixelFormat(int format) {
        return getNativePixelFormat(static_cast<AVPixelFormat>(format));
    }

    VAAPITranscoderImplPrivate::PacketFetcher::PacketFetcher(VAAPITranscoderImplPrivate *p) : p(p) {
    }

    void VAAPITranscoderImplPrivate::PacketFetcher::run() {
        int ret;
        char strBuf[256];
        while (!isInterruptionRequested()) {
            if (p->encodeContext) {
                AVPacket *packet = av_packet_alloc();
                {
                    QMutexLocker locker(&p->encoderMutex);
                    ret = avcodec_receive_packet(p->encodeContext, packet);
                }
                if (ret == 0) {
                    QMutexLocker outputLock(&p->outputQueueMutex);
                    p->outputQueue.enqueue(packet);
                } else if (ret == AVERROR(EAGAIN)) {
                    msleep(2);
                    av_packet_free(&packet);
                } else if (ret == AVERROR_EOF) {
                    av_packet_free(&packet);
                    break;
                } else {
                    av_strerror(ret, strBuf, sizeof(strBuf));
                    qWarning() << "Error while receiving packet:" << strBuf;
                    av_packet_free(&packet);
                    break;
                }
            }
        }
    }
}// namespace AVQt
