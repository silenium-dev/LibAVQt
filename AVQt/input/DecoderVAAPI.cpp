//
// Created by silas on 3/1/21.
//

#include "private/DecoderVAAPI_p.h"
#include "DecoderVAAPI.h"
#include "../output/IFrameSink.h"
#include "../output/IAudioSink.h"

#include <QApplication>
#include <QImage>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define NOW() std::chrono::high_resolution_clock::now();
#define TIME_US(t1, t2) std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()
#endif

void ffmpeg_dump_yuv(const char *filename, AVFrame *avFrame) {
    FILE *fDump = fopen(filename, "ab");

    uint32_t pitchY = avFrame->linesize[0];
    uint32_t pitchU = avFrame->linesize[1];
    uint32_t pitchV = avFrame->linesize[2];

    uint8_t *avY = avFrame->data[0];
    uint8_t *avU = avFrame->data[1];
    uint8_t *avV = avFrame->data[2];

    for (uint32_t i = 0; i < avFrame->height; i++) {
        fwrite(avY, avFrame->width, 1, fDump);
        avY += pitchY;
    }

    for (uint32_t i = 0; i < avFrame->height / 2; i++) {
        fwrite(avU, avFrame->width / 2, 1, fDump);
        avU += pitchU;
    }

    for (uint32_t i = 0; i < avFrame->height / 2; i++) {
        fwrite(avV, avFrame->width / 2, 1, fDump);
        avV += pitchV;
    }

    fclose(fDump);
}

namespace AVQt {
    DecoderVAAPI::DecoderVAAPI(QIODevice *inputDevice, QObject *parent) : QThread(parent), d_ptr(new DecoderVAAPIPrivate(this)) {
        Q_D(AVQt::DecoderVAAPI);

        d->m_inputDevice = inputDevice;
    }

    DecoderVAAPI::DecoderVAAPI(DecoderVAAPIPrivate &p) : d_ptr(&p) {

    }

    DecoderVAAPI::~DecoderVAAPI() {
        delete d_ptr;
        d_ptr = nullptr;
    }

    int DecoderVAAPI::init() {
        Q_D(AVQt::DecoderVAAPI);
        qDebug() << "Initializing VAAPI decoder";

        d->m_pIOBuffer = static_cast<uint8_t *>(av_malloc(32 * 1024));

        d->m_pInputCtx = avio_alloc_context(d->m_pIOBuffer, 32 * 1024, 0, d->m_inputDevice, &DecoderVAAPIPrivate::readFromIO, nullptr,
                                            &DecoderVAAPIPrivate::seekIO);

        d->m_pFormatCtx = avformat_alloc_context();

        d->m_pFormatCtx->pb = d->m_pInputCtx;

        d->m_pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

        if (avformat_open_input(&d->m_pFormatCtx, "", nullptr, nullptr) < 0) {
            qDebug() << "Error opening format context";
            return -1;
        }

//        int m_videoIndex = -1, d->m_audioIndex = -1;

        for (int i = 0; i < d->m_pFormatCtx->nb_streams; ++i) {
            qDebug("Stream %d has content type: %s", i,
                   (d->m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ? "Video" : "Audio"));
            if (d->m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                d->m_videoIndex = i;
                if (d->m_audioIndex >= 0) {
                    break;
                } else {
                    continue;
                }
            }
            if (d->m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                d->m_audioIndex = i;
                if (d->m_videoIndex >= 0) {
                    break;
                } else {
                    continue;
                }
            }
        }

        avformat_find_stream_info(d->m_pFormatCtx, nullptr);

        if (d->m_videoIndex >= 0) {
            d->m_pVideoCodec = avcodec_find_decoder(d->m_pFormatCtx->streams[d->m_videoIndex]->codecpar->codec_id);
            d->m_pVideoCodecCtx = avcodec_alloc_context3(d->m_pVideoCodec);
            avcodec_parameters_to_context(d->m_pVideoCodecCtx, d->m_pFormatCtx->streams[d->m_videoIndex]->codecpar);

            av_hwdevice_ctx_create(&d->m_pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
//            pFramesCtx = av_hwframe_ctx_alloc(d->m_pDeviceCtx);
//            av_hwframe_ctx_init(pFramesCtx);
            d->m_pVideoCodecCtx->hw_device_ctx = d->m_pDeviceCtx;
//            d->m_pVideoCodecCtx->hw_frames_ctx = pFramesCtx;
//            auto hwconfig = avcodec_get_hw_config(d->m_pVideoCodec, 0);
//            auto hwconstrains = av_hwdevice_get_hwframe_constraints(d->m_pDeviceCtx, hwconfig);

            avcodec_open2(d->m_pVideoCodecCtx, d->m_pVideoCodec, nullptr);
            qDebug("Decoder timebase: %d/%d", d->m_pVideoCodecCtx->time_base.num, d->m_pVideoCodecCtx->time_base.den);
            qDebug("Decoder framerate: %d/%d", d->m_pVideoCodecCtx->framerate.num, d->m_pVideoCodecCtx->framerate.den);
        }

        if (d->m_audioIndex >= 0) {
            d->m_pAudioCodec = avcodec_find_decoder(d->m_pFormatCtx->streams[d->m_audioIndex]->codecpar->codec_id);
            d->m_pAudioCodecCtx = avcodec_alloc_context3(d->m_pAudioCodec);
            avcodec_parameters_to_context(d->m_pAudioCodecCtx, d->m_pFormatCtx->streams[d->m_audioIndex]->codecpar);
            avcodec_open2(d->m_pAudioCodecCtx, d->m_pAudioCodec, nullptr);
        }

        for (const auto &cb: d->m_avfCallbacks) {
            cb->init(d->m_pFormatCtx->duration * 1000.0 / AV_TIME_BASE);
        }
        for (const auto &cb: d->m_qiCallbacks) {
            cb->init(d->m_pFormatCtx->duration * 1000.0 / AV_TIME_BASE);
        }
        for (const auto &cb: d->m_audioCallbacks) {
            cb->init(d->m_pFormatCtx->duration * 1000.0 / AV_TIME_BASE);
        }

        return 0;
    }

    int DecoderVAAPI::deinit() {
        Q_D(AVQt::DecoderVAAPI);
        avcodec_close(d->m_pAudioCodecCtx);
        avcodec_free_context(&d->m_pAudioCodecCtx);
        avcodec_free_context(&d->m_pVideoCodecCtx);
        avcodec_close(d->m_pVideoCodecCtx);
        avformat_close_input(&d->m_pFormatCtx);
        avformat_free_context(d->m_pFormatCtx);
        avio_context_free(&d->m_pInputCtx);
//        qDebug() << "DecoderVAAPI deinitialized";
        return 0;
    }

    int DecoderVAAPI::start() {
        Q_D(AVQt::DecoderVAAPI);
        if (!d->m_isRunning) {
            d->m_isRunning.store(true);
            QThread::start(QThread::TimeCriticalPriority);
            return 0;
        } else {
            return -1;
        }
    }

    int DecoderVAAPI::stop() {
        Q_D(AVQt::DecoderVAAPI);
        if (d->m_isRunning) {
            d->m_isRunning.store(false);
            wait();
//            qDebug() << "DecoderVAAPI stopped";
            return 0;
        } else {
            return -1;
        }
    }

    void DecoderVAAPI::pause(bool pause) {
        Q_D(AVQt::DecoderVAAPI);
        if (d->m_isRunning.load() != pause) {
            d->m_isPaused.store(pause);
            paused(pause);
        }
    }

    bool DecoderVAAPI::isPaused() {
        Q_D(AVQt::DecoderVAAPI);
        return d->m_isPaused.load();
    }

    int DecoderVAAPI::registerCallback(IFrameSink *frameSink, uint8_t type) {
        Q_D(AVQt::DecoderVAAPI);
        int result = -1;
        if (type & CB_AVFRAME) {
            QMutexLocker lock(&d->m_avfMutex);
            d->m_avfCallbacks.append(frameSink);
            result = static_cast<int>(d->m_avfCallbacks.indexOf(frameSink));
        }
        if (type & CB_QIMAGE) {
            QMutexLocker lock(&d->m_qiMutex);
            if (result == -1) {
                result = 0;
            }
            d->m_qiCallbacks.append(frameSink);
            result += static_cast<int>((d->m_qiCallbacks.indexOf(frameSink) & 0xFF) << 8);
        }
        return result;
    }

    int DecoderVAAPI::unregisterCallback(IFrameSink *frameSink) {
        Q_D(AVQt::DecoderVAAPI);
        QMutexLocker lock(&d->m_avfMutex);
        int count = d->m_avfCallbacks.removeAll(frameSink);
        count += d->m_qiCallbacks.removeAll(frameSink);
        return count + static_cast<int>(d->m_qiCallbacks.removeAll(frameSink));
    }

    int DecoderVAAPI::registerCallback(IAudioSink *audioSink) {
        Q_D(AVQt::DecoderVAAPI);
        QMutexLocker lock(&d->m_audioMutex);
        d->m_audioCallbacks.append(audioSink);
        return d->m_audioCallbacks.indexOf(audioSink);
    }

    int DecoderVAAPI::unregisterCallback(IAudioSink *audioSink) {
        Q_D(AVQt::DecoderVAAPI);
        QMutexLocker lock(&d->m_audioMutex);
        return d->m_audioCallbacks.removeAll(audioSink);
    }

    int DecoderVAAPIPrivate::readFromIO(void *opaque, uint8_t *buf, int bufSize) {
        auto *inputDevice = static_cast<QIODevice *>(opaque);

        auto bytesRead = inputDevice->read(reinterpret_cast<char *>(buf), bufSize);
        if (bytesRead == 0) {
            return AVERROR_EOF;
        } else {
            return bytesRead;
        }
    }

    int64_t DecoderVAAPIPrivate::seekIO(void *opaque, int64_t pos, int whence) {
        auto *inputDevice = static_cast<QIODevice *>(opaque);

        if (inputDevice->isSequential()) {
            return -1;
        }

        bool result;
        switch (whence) {
            case SEEK_SET:
                result = inputDevice->seek(pos);
                break;
            case SEEK_CUR:
                result = inputDevice->seek(inputDevice->pos() + pos);
                break;
            case SEEK_END:
                result = inputDevice->seek(inputDevice->size() - pos);
                break;
            case AVSEEK_SIZE:
                return inputDevice->size();
            default:
                return -1;
        }

        if (result) {
            return inputDevice->pos();
        } else {
            return -1;
        }
    }

    void DecoderVAAPI::run() {
        Q_D(AVQt::DecoderVAAPI);
        while (d->m_isRunning.load()) {
            if (!d->m_isPaused.load()) {
                auto t1 = NOW();
                AVPacket *packet = av_packet_alloc();
                AVFrame *hwFrame = av_frame_alloc();

                int ret = av_read_frame(d->m_pFormatCtx, packet);
                if (ret < 0) {
                    av_packet_free(&packet);
                    qWarning() << "Error while getting packet from AVFormatContext:" << av_make_error_string(new char[32], 32, ret);
                    return;
                }

                if (packet->stream_index == d->m_videoIndex) {
                    auto t10 = NOW();
                    ret = avcodec_send_packet(d->m_pVideoCodecCtx, packet);
                    if (ret < 0) {
                        av_packet_free(&packet);
                        qWarning() << "Error sending packet to video decoder:" << av_make_error_string(new char[128], 128, ret);
                        return;
                    }
                    auto t11 = NOW();
                    qDebug("Send time: %ld us", TIME_US(t10, t11));

                    while (true) {
                        auto t3 = NOW();
                        if (!(hwFrame = av_frame_alloc()) || !(d->m_pCurrentVideoFrame = av_frame_alloc())) {
                            av_packet_free(&packet);
                            qFatal("Error allocating frames");
                        }

                        ret = avcodec_receive_frame(d->m_pVideoCodecCtx, hwFrame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            av_frame_free(&hwFrame);
//                            av_freep(hwFrame->data[0]);
                            av_frame_free(&d->m_pCurrentVideoFrame);
                            av_packet_free(&packet);
                            break;
                        } else if (ret < 0) {
                            av_packet_free(&packet);
                            qWarning() << "Error receiving frame from decoder:" << av_make_error_string(new char[128], 128, ret);
                            return;
                        }
                        auto t12 = NOW();
                        qDebug("Receive time: %ld us", TIME_US(t3, t12));

                        char strbuf[32];
                        qDebug("Frame pix fmt: %s", av_get_pix_fmt_string(strbuf, 32,
                                                                          static_cast<AVPixelFormat>(reinterpret_cast<AVHWFramesContext *>(hwFrame->hw_frames_ctx->data)->sw_format)));

                        if (hwFrame->format == AV_PIX_FMT_VAAPI) {
//                            ret = av_hwframe_map(d->m_pCurrentVideoFrame, hwFrame, AV_HWFRAME_MAP_READ);
                            ret = av_hwframe_transfer_data(d->m_pCurrentVideoFrame, hwFrame, 0);
                            if (ret < 0) {
                                qFatal("%d: Error mapping frame from gpu: %s", ret, av_make_error_string(new char[128], 128, ret));
                            }
//                            qDebug() << hwFrame->hw_frames_ctx->data;
                        } else {
                            qDebug() << "No hw frame";
                            av_frame_free(&d->m_pCurrentVideoFrame);
                            d->m_pCurrentVideoFrame = hwFrame;
                        }
                        auto t4 = NOW();
                        qDebug("Transfer time: %ld us", TIME_US(t12, t4));

//                        char strbuf[32];
//                        qDebug() << "                " << av_get_pix_fmt_string(strbuf, 32, AV_PIX_FMT_NONE);
//                        qDebug() << "Sw Frame format:"
//                                 << av_get_pix_fmt_string(strbuf, 32, static_cast<AVPixelFormat>(d->m_pCurrentFrame->format));

//                        ffmpeg_dump_yuv("frame.yuv", d->m_pCurrentVideoFrame);

//                        qFatal("");

                        if (!d->m_qiCallbacks.isEmpty()) {
                            if (!d->m_pSwsContext) {
                                d->m_pSwsContext = sws_getContext(d->m_pCurrentVideoFrame->width, d->m_pCurrentVideoFrame->height,
                                                                  static_cast<AVPixelFormat>(d->m_pCurrentVideoFrame->format),
                                                                  d->m_pCurrentVideoFrame->width,
                                                                  d->m_pCurrentVideoFrame->height, AV_PIX_FMT_BGRA, 0, nullptr, nullptr,
                                                                  nullptr);
                            }
                            d->m_pCurrentBGRAFrame = av_frame_alloc();

                            d->m_pCurrentBGRAFrame->width = hwFrame->width;
                            d->m_pCurrentBGRAFrame->height = hwFrame->height;
                            d->m_pCurrentBGRAFrame->format = AV_PIX_FMT_BGRA;

                            av_image_alloc(d->m_pCurrentBGRAFrame->data, d->m_pCurrentBGRAFrame->linesize, d->m_pCurrentVideoFrame->width,
                                           d->m_pCurrentVideoFrame->height, AV_PIX_FMT_BGRA, 1);

//                            qDebug() << "Scaling...";
                            sws_scale(d->m_pSwsContext, d->m_pCurrentVideoFrame->data, d->m_pCurrentVideoFrame->linesize, 0,
                                      d->m_pCurrentVideoFrame->height,
                                      d->m_pCurrentBGRAFrame->data,
                                      d->m_pCurrentBGRAFrame->linesize);
                            QImage frame(d->m_pCurrentBGRAFrame->data[0], d->m_pCurrentVideoFrame->width, d->m_pCurrentVideoFrame->height,
                                         QImage::Format_ARGB32);

                            for (const auto &cb: d->m_qiCallbacks) {
//                                QtConcurrent::run([&](const QImage &image) {
                                cb->onFrame(frame.copy(),
                                            av_q2d(av_inv_q(d->m_pFormatCtx->streams[d->m_videoIndex]->avg_frame_rate)) * 1000.0);
//                                }, frame.copy());
                            }
                            av_freep(d->m_pCurrentBGRAFrame->data);
                            av_frame_free(&d->m_pCurrentBGRAFrame);
                            d->m_pCurrentBGRAFrame = nullptr;
                        }

                        if (!d->m_avfCallbacks.isEmpty()) {
                            for (const auto &cb: d->m_avfCallbacks) {
                                AVFrame *cbFrame = av_frame_alloc();
                                av_frame_ref(cbFrame, d->m_pCurrentVideoFrame);
//                        cbFrame->hw_frames_ctx = av_buffer_ref(hwFrame->hw_frames_ctx);
//                        av_image_alloc(cbFrame->data, cbFrame->linesize, d->m_pCurrentFrame->width, d->m_pCurrentFrame->height,
//                                       static_cast<AVPixelFormat>(d->m_pCurrentFrame->format), 1);
//                        av_frame_copy_props(cbFrame, d->m_pCurrentFrame);
//                        av_frame_copy(cbFrame, d->m_pCurrentFrame);
                                cbFrame->format = d->m_pCurrentVideoFrame->format;
                                cbFrame->width = d->m_pCurrentVideoFrame->width;
                                cbFrame->height = d->m_pCurrentVideoFrame->height;
                                cbFrame->pkt_dts = hwFrame->pkt_dts;
                                cbFrame->pkt_size = hwFrame->pkt_size;
                                cbFrame->pkt_duration = hwFrame->pkt_duration;
                                cbFrame->pkt_pos = hwFrame->pkt_pos;
                                cbFrame->pts = hwFrame->pts;
//                                QtConcurrent::run([&](AVFrame *avFrame) {
                                t3 = NOW();
                                cb->onFrame(cbFrame, d->m_pVideoCodecCtx->time_base, d->m_pVideoCodecCtx->framerate,
                                            av_q2d(av_inv_q(d->m_pFormatCtx->streams[d->m_videoIndex]->avg_frame_rate)) * 1000.0);
//                                }, cbFrame);
                                t4 = NOW();
                                qDebug("CB time: %ld us", TIME_US(t3, t4));
                                av_frame_unref(cbFrame);
                            }
                        }

                        av_frame_free(&hwFrame);
                        av_frame_unref(d->m_pCurrentVideoFrame);
                        d->m_pCurrentVideoFrame = nullptr;
                        hwFrame = nullptr;
                    }
                } else if (packet->stream_index == d->m_audioIndex) {
//                    qDebug() << "Audio packet";
//                    ret = avcodec_send_packet(d->m_pAudioCodecCtx, packet);
//                    if (ret < 0) {
//                        av_packet_free(&packet);
//                        qWarning() << "Error sending packet to audio decoder:" << av_make_error_string(new char[128], 128, ret);
//                        return;
//                    }
//                    while (true) {
//                        auto t1 = NOW();
//                        if (!(d->m_pCurrentAudioFrame = av_frame_alloc())) {
//                            av_packet_free(&packet);
//                            qFatal("Could not allocate audio frame");
//                        }
//
//                        ret = avcodec_receive_frame(d->m_pAudioCodecCtx, d->m_pCurrentAudioFrame);
//                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
//                            av_frame_free(&d->m_pCurrentAudioFrame);
//                            av_packet_free(&packet);
//                            break;
//                        } else if (ret < 0) {
//                            av_packet_free(&packet);
//                            qFatal("%d, Error receiving frame from decoder: %s", ret, av_make_error_string(new char[128], 128, ret));
//                        }
//
//                        {
//                            QMutexLocker lock(&d->m_audioMutex);
//                            for (const auto &cb: d->m_audioCallbacks) {
//                                AVFrame *cbFrame = av_frame_alloc();
//                                av_frame_ref(cbFrame, d->m_pCurrentAudioFrame);
//                                qDebug("Audio sample format: %s", av_get_sample_fmt_name(d->m_pAudioCodecCtx->sample_fmt));
//                                cb->onAudioFrame(cbFrame,
//                                                 cbFrame->pkt_duration * av_q2d(d->m_pFormatCtx->streams[d->m_audioIndex]->time_base) *
//                                                 1000000.0, d->m_pAudioCodecCtx->sample_fmt);
//                                av_frame_unref(cbFrame);
//                            }
//                        }
//                    }
                }
//                if (packet) {
                auto t2 = NOW();
//                    auto duration = packet->duration * 1000000.0 *
//                                    av_q2d(d->m_pFormatCtx->streams[packet->stream_index]->time_base) - TIME_US(t1, t2);
//                    duration = (duration < 0 ? 0 : duration);
                qDebug("Decoder Packettime: %ld us", TIME_US(t1, t2));
//                    usleep(duration);
                av_packet_free(&packet);
//                }
            } else {
                msleep(2);
            }
        }
    }
}