//
// Created by silas on 3/3/21.
//

#include "EncoderVAAPI.h"
#include "private/EncoderVAAPI_p.h"
#include "../input/IFrameSource.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h> // NOLINT(modernize-deprecated-headers)
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#define NOW() std::chrono::high_resolution_clock::now()
#define TIME_US(t1, t2) std::chrono::duration_cast<std::chrono::microseconds>((t2) - (t1)).count()
#define TIME_S(t1, t2) std::chrono::duration_cast<std::chrono::seconds>((t2) - (t1)).count()

#include <QtConcurrent>

namespace AVQt {
    EncoderVAAPI::EncoderVAAPI(QIODevice *outputDevice, VIDEO_CODEC videoCodec, AUDIO_CODEC audioCodec, int width, int height,
                               AVRational framerate, int bitrate, QObject *parent) :
            QThread(parent),
            d_ptr(new EncoderVAAPIPrivate(this)) {
        Q_D(AVQt::EncoderVAAPI);

        d->m_outputDevice = outputDevice;
        d->m_audioCodec = audioCodec;
        d->m_videoCodec = videoCodec;
        d->m_bitrate = bitrate;
        d->m_frameRate = framerate;
        d->m_width = width;
        d->m_height = height;

        if (!d->m_outputDevice->isOpen()) {
            d->m_outputDevice->open(QIODevice::WriteOnly);
        }
    }

    EncoderVAAPI::EncoderVAAPI(EncoderVAAPIPrivate &p) : d_ptr(&p) {

    }

    int EncoderVAAPI::init() {
        Q_D(AVQt::EncoderVAAPI);
        d->pIOBuffer = reinterpret_cast<uint8_t *>(av_malloc(32 * 1024));
        d->pOutputCtx = avio_alloc_context(d->pIOBuffer, 32 * 1024, 1, d->m_outputDevice, nullptr,
                                           &EncoderVAAPIPrivate::writeToIO, nullptr);

        if (!d->pOutputCtx) {
            qFatal("Could not allocate avio contexts");
        }

        if (avformat_alloc_output_context2(&d->pFormatCtx, av_guess_format("mpegts", "", nullptr), "mpegts", "") < 0) {
            qFatal("Could not allocate output context");
        }

        d->pFormatCtx->pb = d->pOutputCtx;
        d->pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

        switch (d->m_videoCodec) {
            case VIDEO_CODEC::H264:
                d->pVideoCodec = avcodec_find_encoder_by_name("h264_vaapi");
                break;
            case VIDEO_CODEC::HEVC:
                d->pVideoCodec = avcodec_find_encoder_by_name("hevc_vaapi");
                break;
            case VIDEO_CODEC::VP9:
                d->pVideoCodec = avcodec_find_encoder_by_name("vp9_vaapi");
                break;
        }

        switch (d->m_audioCodec) {
            case AUDIO_CODEC::AAC:
            case AUDIO_CODEC::OGG:
            case AUDIO_CODEC::OPUS:
                break;
        }

        return 0;
    }

    int EncoderVAAPI::deinit() {
        Q_D(AVQt::EncoderVAAPI);
        if (d->pCurrentFrame) {
            av_frame_free(&d->pCurrentFrame);
        }
        if (d->pVideoCodecCtx) {
            if (avcodec_is_open(d->pVideoCodecCtx)) {
                avcodec_close(d->pVideoCodecCtx);
            }
            avcodec_free_context(&d->pVideoCodecCtx);
            d->pVideoCodecCtx = nullptr;
        }
        if (d->pAudioCodecCtx) {
            if (avcodec_is_open(d->pAudioCodecCtx)) {
                avcodec_close(d->pAudioCodecCtx);
            }
            avcodec_free_context(&d->pAudioCodecCtx);
            d->pAudioCodecCtx = nullptr;
        }
        if (d->pVideoStream) {
            d->pVideoStream = nullptr;
        }
        if (d->pAudioStream) {
            d->pAudioStream = nullptr;
        }

        avformat_free_context(d->pFormatCtx);

        return 0;
    }

    int EncoderVAAPI::start() {
        Q_D(AVQt::EncoderVAAPI);
        if (!d->m_isRunning.load()) {
            d->m_isRunning.store(true);
            QThread::start();
        }
        return 0;
    }

    int EncoderVAAPI::stop() {
        Q_D(AVQt::EncoderVAAPI);
        if (d->m_isRunning.load()) {
            d->m_isRunning.store(false);
            while (!isFinished()) {
                QThread::msleep(1);
            }
            avformat_flush(d->pFormatCtx);
            return 0;
        }
        return -1;
    }

    void EncoderVAAPI::pause(bool pause) {
        Q_D(AVQt::EncoderVAAPI);
        d->m_isPaused.store(pause);
    }

    bool EncoderVAAPI::isPaused() {
        Q_D(AVQt::EncoderVAAPI);
        return d->m_isPaused.load();
    }

    void EncoderVAAPI::onFrame(QImage frame, AVRational timebase, AVRational framerate) {
        Q_D(AVQt::EncoderVAAPI);
        QMutexLocker lock(&d->m_cbTypeMutex);
        if (d->m_cbType == IFrameSource::CB_NONE || d->m_cbType == IFrameSource::CB_QIMAGE) {
            d->m_cbType = IFrameSource::CB_QIMAGE;
            lock.unlock();
            auto *nextFrame = new EncoderVAAPIPrivate::Frame;
            nextFrame->frame = av_frame_alloc();
            nextFrame->timeBase = timebase;
            nextFrame->framerate = framerate;
            av_image_alloc(nextFrame->frame->data, nextFrame->frame->linesize, frame.width(), frame.height(), AV_PIX_FMT_BGRA, 1);
            while (d->m_frameQueue.size() >= 100) {
                QThread::msleep(1);
            }
            av_image_copy_plane(nextFrame->frame->data[0], nextFrame->frame->linesize[0], frame.constBits(), frame.sizeInBytes(),
                                frame.bytesPerLine(), frame.height());
            QMutexLocker queueLock(&d->m_frameQueueMutex);
            while (d->m_frameQueue.size() >= 100) {
                queueLock.unlock();
                QThread::msleep(4);
                queueLock.relock();
            }
            d->m_frameQueue.enqueue(nextFrame);
        } else {
            lock.unlock();
            qWarning("Do NOT use the same encoder instance as callback for both input types");
        }
    }

    void EncoderVAAPI::onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) {
        Q_D(AVQt::EncoderVAAPI);
        QMutexLocker lock(&d->m_cbTypeMutex);
        if (d->m_cbType == IFrameSource::CB_NONE || d->m_cbType == IFrameSource::CB_AVFRAME) {
            d->m_cbType = IFrameSource::CB_AVFRAME;
            lock.unlock();
            auto *nextFrame = new EncoderVAAPIPrivate::Frame;
            nextFrame->frame = av_frame_alloc();
            av_frame_ref(nextFrame->frame, frame);
            nextFrame->framerate = framerate;
            nextFrame->timeBase = timebase;
            while (d->m_frameQueue.size() >= 100) {
                QThread::msleep(4);
            }
            QMutexLocker queueLock(&d->m_frameQueueMutex);
            d->m_frameQueue.enqueue(nextFrame);
        } else {
            lock.unlock();
            qWarning("Do NOT use the same encoder instance as callback for both input types,"
                     "it will result in invalid frame indices and errors in encoding");
        }
    }

    void EncoderVAAPI::run() {
        Q_D(AVQt::EncoderVAAPI);
        std::chrono::time_point<std::chrono::high_resolution_clock> lastPoint, startPoint;
        while (d->m_isRunning.load()) {
            EncoderVAAPIPrivate::Frame *queueFrame = nullptr;
            if (d->m_frameQueueMutex.tryLock()) {
                if (!d->m_frameQueue.isEmpty()) {
                    queueFrame = d->m_frameQueue.dequeue();
                    d->m_frameQueueMutex.unlock();
                } else {
                    d->m_frameQueueMutex.unlock();
                    msleep(4);
                    continue;
                }
            } else {
                msleep(4);
                continue;
            }

            int ret = 0;
            constexpr int strBufSize = 64;
            char strBuf[strBufSize];
            auto inputFormat = static_cast<AVPixelFormat>(queueFrame->frame->format);
            auto inputWidth = queueFrame->frame->width;
            auto inputHeight = queueFrame->frame->height;
            if (d->m_isFirstFrame.load()) {
                d->m_isFirstFrame.store(false);
                // Init HW device context
                if ((ret = av_hwdevice_ctx_create(&d->pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0)) < 0) {
                    qFatal("%d: Could not create AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }

                // Init HW frames context
                d->pFramesCtx = av_hwframe_ctx_alloc(d->pDeviceCtx);
                if (!d->pFramesCtx) {
                    qFatal("Could not allocate AVHWFramesContext");
                }

                printf("Source framerate: %d/%d\n", queueFrame->framerate.num, queueFrame->framerate.den);
                printf("Source timebase: %d/%d\n", queueFrame->timeBase.num, queueFrame->timeBase.den);

                auto framesCtx = reinterpret_cast<AVHWFramesContext *>(d->pFramesCtx->data);

                framesCtx->format = AV_PIX_FMT_VAAPI;
                framesCtx->sw_format = AV_PIX_FMT_NV12;
                framesCtx->width = queueFrame->frame->width;
                framesCtx->height = queueFrame->frame->height;
                framesCtx->initial_pool_size = 20;

                if ((ret = av_hwframe_ctx_init(d->pFramesCtx)) < 0) {
                    qFatal("%d: Could not initialize AVHWFramesContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }

                // Initialize video encoder
                d->pVideoCodecCtx = avcodec_alloc_context3(d->pVideoCodec);
                AVCodecParameters *params = avcodec_parameters_alloc();

                params->format = AV_PIX_FMT_VAAPI;
                params->bit_rate = 5 * 1024 * 1024;
                params->width = framesCtx->width;
                params->height = framesCtx->height;

                // Use full color range for clear colors
                params->codec_id = d->pVideoCodec->id;
                params->codec_type = d->pVideoCodec->type;
                params->color_range = AVCOL_RANGE_JPEG;
                params->color_primaries = AVCOL_PRI_BT2020;
                params->color_trc = AVCOL_TRC_SMPTE2084;
                params->color_space = AVCOL_SPC_BT2020_NCL;

                avcodec_parameters_to_context(d->pVideoCodecCtx, params);

                // Create new stream in the AVFormatContext
                d->pVideoStream = avformat_new_stream(d->pFormatCtx, d->pVideoCodec);
                avcodec_parameters_from_context(d->pVideoStream->codecpar, d->pVideoCodecCtx);
                d->pVideoStream->time_base = queueFrame->timeBase;
                d->pVideoStream->avg_frame_rate = queueFrame->framerate;

                if ((ret = avformat_init_output(d->pFormatCtx, nullptr)) < 0) {
                    qFatal("%d: Could not open output context: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }

                // Finish codec context initialization
                d->pVideoCodecCtx->hw_device_ctx = av_buffer_ref(d->pDeviceCtx);
                d->pVideoCodecCtx->hw_frames_ctx = av_buffer_ref(d->pFramesCtx);
                d->pVideoCodecCtx->framerate = d->pVideoStream->avg_frame_rate;
                d->pVideoCodecCtx->time_base = d->pVideoStream->time_base;

                if ((ret = avcodec_open2(d->pVideoCodecCtx, d->pVideoCodec, nullptr)) < 0) {
                    qFatal("%d: Could not open video encoder \"%s\": %s", ret, d->pVideoCodec->long_name,
                           av_make_error_string(strBuf, strBufSize, ret));
                }

                printf("Encoder framerate: %d/%d\n", d->pVideoStream->avg_frame_rate.num, d->pVideoStream->avg_frame_rate.den);
                printf("Encoder timebase: %d/%d\n", d->pVideoStream->time_base.num, d->pVideoStream->time_base.den);

                // Create frame on GPU
                d->pHWFrame = av_frame_alloc();
                d->pHWFrame->width = framesCtx->width;
                d->pHWFrame->height = framesCtx->height;
                d->pHWFrame->format = AV_PIX_FMT_VAAPI;
                d->pHWFrame->hw_frames_ctx = av_buffer_ref(d->pFramesCtx);
                d->pHWFrame->color_range = d->pVideoCodecCtx->color_range;
                d->pHWFrame->color_trc = d->pVideoCodecCtx->color_trc;
                d->pHWFrame->color_primaries = d->pVideoCodecCtx->color_primaries;
                d->pHWFrame->colorspace = d->pVideoCodecCtx->colorspace;

                if ((ret = av_hwframe_get_buffer(d->pFramesCtx, d->pHWFrame, 0)) < 0) {
                    qFatal("%d: Could not allocate frame on GPU: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }

                // Pixel format conversion is only needed, if the input format and the upload one are different
                if (inputFormat != framesCtx->sw_format) {
                    d->pSwsContext = sws_getContext(inputWidth, inputHeight, inputFormat, d->pVideoCodecCtx->width,
                                                    d->pVideoCodecCtx->height, framesCtx->sw_format, 0, nullptr, nullptr, nullptr);
                }

                printf("\n");
                QtConcurrent::run([&] {
                    // Receive and write all encoded packets
                    AVPacket *packet = av_packet_alloc();
                    while (d->m_isRunning.load()) {
                        while (true) {
                            ret = avcodec_receive_packet(d->pVideoCodecCtx, packet);
                            // If ret != 0, there is no packet available
                            if (ret != 0) {
                                av_packet_unref(packet);
                                break;
                            } else {
                                packet->stream_index = d->pVideoStream->index;
                                // Calculate pts and dts from framenumber, framerate and timebase
                                packet->duration = av_q2d(av_inv_q(d->pVideoStream->avg_frame_rate)) / av_q2d(d->pVideoStream->time_base) / 2.0;
                                packet->dts = packet->duration * 2.0 * (d->m_frameNumber.load() - 0.5);
                                packet->dts = packet->dts < 0 ? 0 : packet->dts;
                                packet->pts = packet->duration * 2.0 * d->m_frameNumber++;
                                ret = av_write_frame(d->pFormatCtx, packet);
                                if (ret < 0) {
                                    char strBuf[64];
                                    qWarning("%d: Could not write packet to output: %s", ret, av_make_error_string(strBuf, 64, ret));
                                }
                                if ((d->m_frameNumber.load() - 1) % 60 == 0) {
                                    printf("Encoded frame %lu\n", d->m_frameNumber.load() - 1);
                                    printf("Encoder framerate: %d/%d\n", d->pVideoStream->avg_frame_rate.num,
                                           d->pVideoStream->avg_frame_rate.den);
                                    printf("Encoder timebase: %d/%d\n", d->pVideoStream->time_base.num, d->pVideoStream->time_base.den);
                                    printf("Packet pts, duration: %ld, %ld\n", packet->pts, packet->duration);
                                    fflush(stdout);
                                }
                                av_packet_unref(packet);
                            }
                            av_packet_unref(packet);
                        }
                        QThread::msleep(6);
                    }

                    // Cleanup
                    av_packet_free(&packet);
                });
                startPoint = NOW();
            }

            // Init frame for upload
            auto t7 = NOW();
            AVFrame *swFrame = av_frame_alloc();

            swFrame->width = d->pHWFrame->width;
            swFrame->height = d->pHWFrame->height;
            swFrame->color_range = d->pVideoCodecCtx->color_range;
            swFrame->color_trc = d->pVideoCodecCtx->color_trc;
            swFrame->color_primaries = d->pVideoCodecCtx->color_primaries;
            swFrame->colorspace = d->pVideoCodecCtx->colorspace;
            swFrame->format = reinterpret_cast<AVHWFramesContext *>(d->pFramesCtx->data)->sw_format;

            // If input pixel format and upload pixel format are different, convert the frame, else ref them
            // The swscale context is nullptr, if the formats are the same
            if (d->pSwsContext) {
                av_image_alloc(swFrame->data, swFrame->linesize, swFrame->width, swFrame->height,
                               static_cast<AVPixelFormat>(swFrame->format),
                               1);
                sws_scale(d->pSwsContext, queueFrame->frame->data, queueFrame->frame->linesize, 0, queueFrame->frame->height,
                          swFrame->data, swFrame->linesize);
            } else {
                av_frame_ref(swFrame, queueFrame->frame);
            }

            // Upload frame to GPU
            if ((ret = av_hwframe_transfer_data(d->pHWFrame, swFrame, 0)) < 0) {
                qFatal("%d: Could not map frame to gpu: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
            }
            if (d->pSwsContext) {
                av_freep(&swFrame->data[0]);
            }
            av_frame_free(&swFrame);
            av_frame_free(&queueFrame->frame);
            queueFrame->frame = nullptr;
            delete queueFrame;

            avcodec_send_frame(d->pVideoCodecCtx, d->pHWFrame);
        }
        printf("Stats\n Average FPS: %0.2f\nAverage frame time: %0.2f us\n", d->m_frameNumber.load() * 1.0 / TIME_S(startPoint, NOW()),
               TIME_US(startPoint, NOW()) * 1.0 / d->m_frameNumber.load());
    }

    int EncoderVAAPIPrivate::writeToIO(void *opaque, uint8_t *buf, int bufSize) {
        auto t1 = NOW();

        auto *outputDevice = static_cast<QIODevice *>(opaque);
        if (!outputDevice->isWritable()) {
            return AVERROR(ENODEV);
        }
        auto bytesWritten = outputDevice->write(reinterpret_cast<const char *>(buf), bufSize);

        auto t2 = NOW();
        return bytesWritten;
    }

    [[maybe_unused]] AVPixelFormat EncoderVAAPIPrivate::getPixelFormat(AVCodecContext *pCodecCtx, const AVPixelFormat *formats) {
        Q_UNUSED(pCodecCtx)

        for (auto fmt = formats; *fmt != AV_PIX_FMT_NONE; ++fmt) {
            if (*fmt == AV_PIX_FMT_VAAPI) {
                return *fmt;
            }
        }
        qWarning() << "Could not get HW surface format";
        return AV_PIX_FMT_NONE;
    }

    [[maybe_unused]] int64_t EncoderVAAPIPrivate::frameNumberToPts(uint64_t frameNumber, AVRational frameRate, AVRational timeBase) {
        auto ptsPerFrame = av_q2d(av_inv_q(frameRate)) * av_q2d(timeBase) / 1000.0;
        return ptsPerFrame * frameNumber;
    }

    EncoderVAAPI::~EncoderVAAPI() {
        Q_D(AVQt::EncoderVAAPI);
        deinit();
        for (const auto &e: d->m_frameQueue) {
            av_frame_free(&e->frame);
        }
        qDeleteAll(d->m_frameQueue);
        d->m_frameQueue.clear();
    }

}