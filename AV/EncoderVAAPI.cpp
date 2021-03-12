//
// Created by silas on 3/3/21.
//

#include "EncoderVAAPI.h"
#include "IFrameSource.h"

#define NOW() std::chrono::high_resolution_clock::now()
#define TIME_US(t1, t2) std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()

#include <QtConcurrent>

namespace AV {
    EncoderVAAPI::EncoderVAAPI(QIODevice *outputDevice, VIDEO_CODEC videoCodec, AUDIO_CODEC audioCodec, int width, int height,
                               AVRational framerate, int bitrate, QObject *parent) :
            QThread(parent),
            m_outputDevice(outputDevice),
            m_audioCodec(audioCodec),
            m_videoCodec(videoCodec),
            m_bitrate(bitrate),
            m_frameRate(framerate),
            m_width(width),
            m_height(height) {
        if (!m_outputDevice->isOpen()) {
            m_outputDevice->open(QIODevice::WriteOnly);
        }
    }

    int EncoderVAAPI::init() {
        pIOBuffer = reinterpret_cast<uint8_t *>(av_malloc(32 * 1024));
        pOutputCtx = avio_alloc_context(pIOBuffer, 32 * 1024, 1, m_outputDevice, nullptr, &EncoderVAAPI::writeToIO, nullptr);

        if (!pOutputCtx) {
            qFatal("Could not allocate avio contexts");
        }

        if (avformat_alloc_output_context2(&pFormatCtx, av_guess_format("mpegts", "", nullptr), "mpegts", "") < 0) {
            qFatal("Could not allocate output context");
        }

        pFormatCtx->pb = pOutputCtx;
        pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

        switch (m_videoCodec) {
            case VIDEO_CODEC::H264:
                pVideoCodec = avcodec_find_encoder_by_name("h264_vaapi");
                break;
            case VIDEO_CODEC::HEVC:
                pVideoCodec = avcodec_find_encoder_by_name("hevc_vaapi");
                break;
            case VIDEO_CODEC::VP9:
                pVideoCodec = avcodec_find_encoder_by_name("vp9_vaapi");
                break;
        }

        switch (m_audioCodec) {

        }

//        pVideoStream = avformat_new_stream(pFormatCtx, pVideoCodec);
//        pVideoStream->codecpar->bit_rate = m_bitrate;
//        pVideoStream->codecpar->format = AV_PIX_FMT_VAAPI;
//        pVideoStream->codecpar->width = m_width;
//        pVideoStream->codecpar->height = m_height;
//        pVideoStream->time_base = av_inv_q(m_frameRate);
////        pVideoStream->codecpar->color_primaries = AVCOL_PRI_BT2020;
////        pVideoStream->codecpar->color_range = AVCOL_RANGE_NB;
////        pVideoStream->codecpar->color_space = AVCOL_SPC_BT2020_NCL;
////        pVideoStream->codecpar->color_trc = AVCOL_TRC_BT2020_10;
//
//        pVideoCodecCtx = avcodec_alloc_context3(pVideoCodec);
////        avcodec_parameters_to_context(pVideoCodecCtx, pVideoStream->codecpar);
//        pVideoCodecCtx->framerate = m_frameRate;
//        pVideoCodecCtx->time_base = av_inv_q(m_frameRate);
//        pVideoCodecCtx->pix_fmt = AV_PIX_FMT_VAAPI;
//        pVideoCodecCtx->width = m_width;
//        pVideoCodecCtx->height = m_height;
//        pVideoCodecCtx->bit_rate = m_bitrate;
//        pVideoCodecCtx->gop_size = m_frameRate.num;
//        pVideoCodecCtx->max_b_frames = 1;
//
//        av_hwdevice_ctx_create(&pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
//        pFramesCtx = av_hwframe_ctx_alloc(pDeviceCtx);
//
//        auto *framesCtx = reinterpret_cast<AVHWFramesContext *>(pFramesCtx->data);
//        framesCtx->height = m_height;
//        framesCtx->width = m_width;
//        framesCtx->format = AV_PIX_FMT_VAAPI;
//        framesCtx->sw_format = AV_PIX_FMT_NV12;
//        framesCtx->initial_pool_size = 20;
//
//        pVideoCodecCtx->hw_frames_ctx = pFramesCtx;
////        pVideoCodecCtx->hw_device_ctx = pDeviceCtx;
//
//        if (avcodec_open2(pVideoCodecCtx, pVideoCodec, nullptr) < 0) {
//            qFatal("Could not open video codec");
//        }

        return 0;
    }

    int EncoderVAAPI::deinit() {
        if (pCurrentFrame) {
            av_frame_free(&pCurrentFrame);
        }
        if (pVideoCodecCtx) {
            if (avcodec_is_open(pVideoCodecCtx)) {
                avcodec_close(pVideoCodecCtx);
            }
            avcodec_free_context(&pVideoCodecCtx);
            pVideoCodecCtx = nullptr;
        }
        if (pAudioCodecCtx) {
            if (avcodec_is_open(pAudioCodecCtx)) {
                avcodec_close(pAudioCodecCtx);
            }
            avcodec_free_context(&pAudioCodecCtx);
            pAudioCodecCtx = nullptr;
        }
        if (pVideoStream) {
            pVideoStream = nullptr;
        }
        if (pAudioStream) {
            pAudioStream = nullptr;
        }

//        avio_close(pOutputCtx);
        avformat_free_context(pFormatCtx);

        return 0;
    }

    int EncoderVAAPI::start() {
        if (!isRunning.load()) {
            isRunning.store(true);
            QThread::start();
        }
        return 0;
    }

    int EncoderVAAPI::stop() {
        if (isRunning.load()) {
            isRunning.store(false);
            while (!isFinished()) {
                QThread::msleep(1);
            }
            av_write_frame(pFormatCtx, nullptr);
            av_write_trailer(pFormatCtx);
            avformat_flush(pFormatCtx);
            return 0;
        }
        return -1;
    }

    int EncoderVAAPI::pause(bool pause) {
        return 0;
    }

    void EncoderVAAPI::onFrame(QImage frame, AVRational timebase, AVRational framerate) {
        QMutexLocker lock(&m_cbTypeMutex);
        if (m_cbType == 0 || m_cbType == IFrameSource::CB_QIMAGE) {
            m_cbType = IFrameSource::CB_QIMAGE;
            lock.unlock();
            auto *nextFrame = new Frame;
            nextFrame->frame = av_frame_alloc();
            nextFrame->timeBase = timebase;
            nextFrame->framerate = framerate;
            av_image_alloc(nextFrame->frame->data, nextFrame->frame->linesize, frame.width(), frame.height(), AV_PIX_FMT_BGRA, 1);
            while (m_frameQueue.size() >= 100) {
                QThread::msleep(1);
            }
            av_image_copy_plane(nextFrame->frame->data[0], nextFrame->frame->linesize[0], frame.constBits(), frame.sizeInBytes(),
                                frame.bytesPerLine(), frame.height());
            QMutexLocker queueLock(&m_frameQueueMutex);
            while (m_frameQueue.size() >= 100) {
                queueLock.unlock();
                QThread::msleep(4);
                queueLock.relock();
            }
            m_frameQueue.enqueue(nextFrame);
        } else {
            lock.unlock();
            qWarning("Do NOT use the same encoder instance as callback for both input types");
        }
    }

    void EncoderVAAPI::onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) {
        QMutexLocker lock(&m_cbTypeMutex);
        if (m_cbType == 0 || m_cbType == IFrameSource::CB_AVFRAME) {
            m_cbType = IFrameSource::CB_AVFRAME;
            lock.unlock();
            auto *nextFrame = new Frame;
            nextFrame->frame = frame;
            nextFrame->framerate = framerate;
            nextFrame->timeBase = timebase;
            while (m_frameQueue.size() >= 100) {
                QThread::msleep(1);
            }
            QMutexLocker queueLock(&m_frameQueueMutex);
            m_frameQueue.enqueue(nextFrame);
        } else {
            lock.unlock();
            qWarning("Do NOT use the same encoder instance as callback for both input types");
        }
    }

    void EncoderVAAPI::run() {
        std::chrono::time_point<std::chrono::high_resolution_clock> lastPoint;
        uint64_t lastFrameNumber = 0;
        while (isRunning.load()) {
            Frame *queueFrame = nullptr;
            {
                QMutexLocker lock(&m_frameQueueMutex);
                if (!m_frameQueue.isEmpty()) {
                    queueFrame = m_frameQueue.dequeue();
                } else {
                    msleep(4);
                    continue;
                }
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            int ret = 0;
            constexpr int strBufSize = 64;
            char strBuf[strBufSize];
            auto inputFormat = static_cast<AVPixelFormat>(queueFrame->frame->format);
            auto inputWidth = queueFrame->frame->width;
            auto inputHeight = queueFrame->frame->height;
            if (m_isFirstFrame.load()) {
                m_isFirstFrame.store(false);
                // Init HW device context
                if ((ret = av_hwdevice_ctx_create(&pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0)) < 0) {
                    qFatal("%d: Could not create AVHWDeviceContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }

                // Init HW frames context
                pFramesCtx = av_hwframe_ctx_alloc(pDeviceCtx);
                if (!pFramesCtx) {
                    qFatal("Could not allocate AVHWFramesContext");
                }

                auto framesCtx = reinterpret_cast<AVHWFramesContext *>(pFramesCtx->data);

                framesCtx->format = AV_PIX_FMT_VAAPI;
                if (m_videoCodec != VIDEO_CODEC::HEVC) {
                    framesCtx->sw_format = AV_PIX_FMT_NV12;
                } else {
                    framesCtx->sw_format = AV_PIX_FMT_P010LE;
                }
                framesCtx->width = queueFrame->frame->width;
                framesCtx->height = queueFrame->frame->height;
                framesCtx->initial_pool_size = 20;

                if ((ret = av_hwframe_ctx_init(pFramesCtx)) < 0) {
                    qFatal("%d: Could not initialize AVHWFramesContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }

                // Initialize video encoder
                pVideoCodecCtx = avcodec_alloc_context3(pVideoCodec);
                AVCodecParameters *params = avcodec_parameters_alloc();

                params->format = AV_PIX_FMT_VAAPI;
                params->bit_rate = 5 * 1024 * 1024;
                params->width = framesCtx->width;
                params->height = framesCtx->height;

                // Use full color range for clear colors
                params->color_primaries = AVCOL_PRI_BT2020;
                params->color_trc = AVCOL_TRC_SMPTE2084;
                params->color_space = AVCOL_SPC_BT2020_NCL;
                params->codec_id = pVideoCodec->id;
                params->codec_type = pVideoCodec->type;
                params->color_range = AVCOL_RANGE_JPEG;

                avcodec_parameters_to_context(pVideoCodecCtx, params);

                pVideoCodecCtx->hw_device_ctx = av_buffer_ref(pDeviceCtx);
                pVideoCodecCtx->hw_frames_ctx = av_buffer_ref(pFramesCtx);
//            pVideoCodecCtx->framerate = av_mul_q(pDecodeStream->avg_frame_rate, av_make_q(1, 2));
                pVideoCodecCtx->framerate = queueFrame->framerate;
                pVideoCodecCtx->time_base = queueFrame->timeBase;
//            pVideoCodecCtx->time_base = av_make_q(1, 1000000);

                if ((ret = avcodec_open2(pVideoCodecCtx, pVideoCodec, nullptr)) < 0) {
                    qFatal("%d: Could not open video encoder \"%s\": %s", ret, pVideoCodec->long_name,
                           av_make_error_string(strBuf, strBufSize, ret));
                }
//            pVideoCodecCtx->time_base = av_make_q(1, 1000000);

                if ((ret = avcodec_open2(pVideoCodecCtx, pVideoCodec, nullptr)) < 0) {
                    qFatal("%d: Could not open video encoder \"%s\": %s", ret, pVideoCodec->long_name,
                           av_make_error_string(strBuf, strBufSize, ret));
                }

                // Create new stream in the AVFormatContext

                // Create new stream in the AVFormatContext
                pVideoStream = avformat_new_stream(pFormatCtx, pVideoCodec);
                avcodec_parameters_from_context(pVideoStream->codecpar, pVideoCodecCtx);
                pVideoStream->time_base = queueFrame->timeBase;
                pVideoStream->avg_frame_rate = queueFrame->framerate;

                // Create frame on GPU
                pHWFrame = av_frame_alloc();
                pHWFrame->width = framesCtx->width;
                pHWFrame->height = framesCtx->height;
                pHWFrame->format = AV_PIX_FMT_VAAPI;
                pHWFrame->hw_frames_ctx = av_buffer_ref(pFramesCtx);
                pHWFrame->color_range = pVideoCodecCtx->color_range;
                pHWFrame->color_trc = pVideoCodecCtx->color_trc;
                pHWFrame->color_primaries = pVideoCodecCtx->color_primaries;
                pHWFrame->colorspace = pVideoCodecCtx->colorspace;

                if ((ret = av_hwframe_get_buffer(pFramesCtx, pHWFrame, 0)) < 0) {
                    qFatal("%d: Could not allocate frame on GPU: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }

                // Pixel format conversion is only needed, if the input format and the upload one are different
                if (inputFormat != framesCtx->sw_format) {
                    pSwsContext = sws_getContext(inputWidth, inputHeight, inputFormat, pVideoCodecCtx->width, pVideoCodecCtx->height,
                                                 framesCtx->sw_format, 0, nullptr, nullptr, nullptr);
                }

                if ((ret = avformat_write_header(pFormatCtx, nullptr)) < 0) {
                    qFatal("%d: Could not open output context: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }
                printf("\n");
//                qDebug() << "First frame";
//                QtConcurrent::run([&] {
//                    // Receive and write all encoded packets
////            QFuture<void> writeFuture = QtConcurrent::run([] {});
//                    while (isRunning.load()) {
//                        AVPacket *packet = av_packet_alloc();
//                        while (true) {
////                ++loopCount;
//                            auto t5 = NOW();
//
//                            ret = avcodec_receive_packet(pVideoCodecCtx, packet);
//
//                            // If ret != 0, there is no packet available
//                            if (ret != 0) {
////                    writeFuture.waitForFinished();
//                                av_packet_unref(packet);
////                                av_packet_free(&packet);
//                                break;
//                            } else {
//                                auto t6 = NOW();
//                                qDebug() << "Received in" << TIME_US(t5, t6) << "us";
//
////            packet->pts = frameNumberToPts(m_frameNumber++, pVideoCodecCtx->framerate, pVideoCodecCtx->time_base) / 2;
////            packet->dts = packet->pts - frameNumberToPts(1, pVideoCodecCtx->framerate, pVideoCodecCtx->time_base);
////            packet->dts = packet->dts <= 0 ? 0 : packet->dts;
////            packet->pts = frameNumberToPts(m_frameNumber++, pVideoStream->avg_frame_rate, pVideoStream->time_base);
////                packet->pts = AV_NOPTS_VALUE;
////                double dts = av_gettime() / 1000.0; // us -> ms
////                dts *= av_q2d(pVideoStream->avg_frame_rate);
////                if (m_dtsOffset < 0) {
////                    m_dtsOffset = dts;
////                }
////                writeFuture.waitForFinished();
//
//
////                qDebug("PTS: %ld, DTS: %ld, duration: %ld", packet->pts, packet->dts, packet->duration);
//
//
////                writeFuture = QtConcurrent::run([this](AVPacket *pkt) {
//                                packet->stream_index = pVideoStream->index;
//                                // Calculate pts and dts from framenumber, framerate and timebase
//                                packet->duration = av_q2d(av_inv_q(pVideoStream->time_base)) / av_q2d(pVideoStream->avg_frame_rate);
//                                packet->dts = packet->duration * (m_frameNumber.load() - 0.1);
//                                packet->dts = packet->dts < 0 ? 0 : packet->dts;
//                                packet->pts = packet->duration * m_frameNumber++;
//                                ret = av_write_frame(pFormatCtx, packet);
//                                if (ret < 0) {
//                                    char strBuf[64];
//                                    qWarning("%d: Could not write packet to output: %s", ret, av_make_error_string(strBuf, 64, ret));
//                                }
//                                av_packet_unref(packet);
//                                if ((m_frameNumber.load() - 1) % 60 == 0) {
//                                    printf("\rEncoded frame %lu\n", m_frameNumber.load() - 1);
//                                    fflush(stdout);
//                                }
//                            }
////                }, packet);
//
////                av_packet_free(&packet);
//                            auto t4 = NOW();
//
//                            printf("Time for receiving and writing packets: %ld us\n", TIME_US(t5, t4));
//                            av_packet_unref(packet);
////                            av_packet_free(&packet);
//                        }
//                        av_packet_free(&packet);
//                        QThread::msleep(4);
//                    }
//                    // Cleanup
//                });
            } else {
//                qDebug() << "Later frame";
            }

//        if (m_frameNumber.load() % 2 == 0) {

            // Init frame for upload
            auto t7 = NOW();
            AVFrame *swFrame = av_frame_alloc();

            swFrame->width = pHWFrame->width;
            swFrame->height = pHWFrame->height;
            swFrame->color_range = pVideoCodecCtx->color_range;
            swFrame->color_trc = pVideoCodecCtx->color_trc;
            swFrame->color_primaries = pVideoCodecCtx->color_primaries;
            swFrame->colorspace = pVideoCodecCtx->colorspace;
            swFrame->format = reinterpret_cast<AVHWFramesContext *>(pFramesCtx->data)->sw_format;

            // If input pixel format and upload pixel format are different, convert the frame, else ref them
            // The swscale context is nullptr, if the formats are the same
            if (pSwsContext) {
                av_image_alloc(swFrame->data, swFrame->linesize, swFrame->width, swFrame->height,
                               static_cast<AVPixelFormat>(swFrame->format),
                               1);
                sws_scale(pSwsContext, queueFrame->frame->data, queueFrame->frame->linesize, 0, queueFrame->frame->height, swFrame->data,
                          swFrame->linesize);
            } else {
                av_frame_ref(swFrame, queueFrame->frame);
            }

            // Upload frame to GPU
            av_hwframe_transfer_data(pHWFrame, swFrame, 0);
            avcodec_send_frame(pVideoCodecCtx, pHWFrame);

            auto t8 = NOW();

            qDebug() << "Time for sending to encoder" << TIME_US(t7, t8) << "us";

            auto t3 = NOW();
            // Receive and write all encoded packets
            AVPacket *packet = av_packet_alloc();
//            QFuture<void> writeFuture = QtConcurrent::run([] {});

//            uint32_t loopCount = 0;

            while (true) {
//                ++loopCount;
                auto t5 = NOW();

                ret = avcodec_receive_packet(pVideoCodecCtx, packet);

                // If ret != 0, there is no packet available
                if (ret != 0) {
//                    writeFuture.waitForFinished();
                    av_packet_unref(packet);
                    break;
                } else {
                    auto t6 = NOW();
                    qDebug() << "Received in" << TIME_US(t5, t6) << "us";

//            packet->pts = frameNumberToPts(m_frameNumber++, pVideoCodecCtx->framerate, pVideoCodecCtx->time_base) / 2;
//            packet->dts = packet->pts - frameNumberToPts(1, pVideoCodecCtx->framerate, pVideoCodecCtx->time_base);
//            packet->dts = packet->dts <= 0 ? 0 : packet->dts;
//            packet->pts = frameNumberToPts(m_frameNumber++, pVideoStream->avg_frame_rate, pVideoStream->time_base);
//                packet->pts = AV_NOPTS_VALUE;
//                double dts = av_gettime() / 1000.0; // us -> ms
//                dts *= av_q2d(pVideoStream->avg_frame_rate);
//                if (m_dtsOffset < 0) {
//                    m_dtsOffset = dts;
//                }
//                writeFuture.waitForFinished();


//                qDebug("PTS: %ld, DTS: %ld, duration: %ld", packet->pts, packet->dts, packet->duration);


//                writeFuture = QtConcurrent::run([this](AVPacket *pkt) {
                    packet->stream_index = pVideoStream->index;
                    // Calculate pts and dts from framenumber, framerate and timebase
                    packet->duration = av_q2d(av_inv_q(pVideoStream->time_base)) / av_q2d(pVideoStream->avg_frame_rate);
                    packet->dts = packet->duration * (m_frameNumber - 0.1);
                    packet->dts = packet->dts < 0 ? 0 : packet->dts;
                    packet->pts = packet->duration * m_frameNumber++;
                    ret = av_write_frame(pFormatCtx, packet);
                    if (ret < 0) {
                        char strBuf[64];
                        qWarning("%d: Could not write packet to output: %s", ret, av_make_error_string(strBuf, 64, ret));
                    }
                    av_packet_unref(packet);
                    if ((m_frameNumber.load() - 1) % 60 == 0) {
                        printf("\rEncoded frame %lu", m_frameNumber.load() - 1);
                        fflush(stdout);
                    }
                }
//                }, packet);

//                av_packet_free(&packet);
            }
            auto t4 = NOW();

            printf("Time for receiving and writing packets: %ld us\n", TIME_US(t3, t4));

//             Cleanup
            av_packet_free(&packet);

            if (pSwsContext) {
                av_freep(swFrame->data);
                av_frame_free(&swFrame);
            } else {
                av_frame_unref(swFrame);
            }
            av_frame_free(&queueFrame->frame);

            // Time measurement
            auto t2 = std::chrono::high_resolution_clock::now();
            printf("Frametime: %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
            if (std::chrono::duration_cast<std::chrono::milliseconds>(t2 - lastPoint).count() >= 1000) {
                printf("FPS: %f\n", (m_frameNumber.load() - lastFrameNumber) * 1.0 /
                                    std::chrono::duration_cast<std::chrono::milliseconds>(t2 - lastPoint).count() * 1000);
                lastPoint = t2;
                lastFrameNumber = m_frameNumber.load() - 1;
            }
        }
    }

    int EncoderVAAPI::writeToIO(void *opaque, uint8_t *buf, int bufSize) {
        auto t1 = NOW();

        auto *outputDevice = static_cast<QIODevice *>(opaque);
        if (!outputDevice->isWritable()) {
            return AVERROR(ENODEV);
        }
        auto bytesWritten = outputDevice->write(reinterpret_cast<const char *>(buf), bufSize);

        auto t2 = NOW();
        qDebug() << "Wrote in" << TIME_US(t1, t2) << "us";
        return bytesWritten;
    }

    AVPixelFormat EncoderVAAPI::getPixelFormat(AVCodecContext *pCodecCtx, const AVPixelFormat *formats) {
        for (auto fmt = formats; *fmt != AV_PIX_FMT_NONE; ++fmt) {
            if (*fmt == AV_PIX_FMT_VAAPI) {
                return *fmt;
            }
        }
        qWarning() << "Could not get HW surface format";
        return AV_PIX_FMT_NONE;
    }

    int64_t EncoderVAAPI::frameNumberToPts(uint64_t frameNumber, AVRational frameRate, AVRational timeBase) {
        auto ptsPerFrame = av_q2d(av_inv_q(frameRate)) * av_q2d(timeBase) / 1000.0;
        return ptsPerFrame * frameNumber;
    }

    EncoderVAAPI::~EncoderVAAPI() {
//        stop();
        deinit();
        for (const auto &e: m_frameQueue) {
            av_frame_free(&e->frame);
        }
        qDeleteAll(m_frameQueue);
        m_frameQueue.clear();
    }

}