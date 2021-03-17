//
// Created by silas on 3/1/21.
//

#include "private/DecoderVAAPI_p.h"
#include "DecoderVAAPI.h"
#include "../output/IFrameSink.h"

#include <QApplication>
#include <QImage>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define NOW() std::chrono::high_resolution_clock::now();
#define TIME_US(t1, t2) std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()
#endif

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
        }

        if (d->m_audioIndex >= 0) {
            d->m_pAudioCodec = avcodec_find_decoder(d->m_pFormatCtx->streams[d->m_audioIndex]->codecpar->codec_id);
            d->m_pAudioCodecCtx = avcodec_alloc_context3(d->m_pAudioCodec);
            avcodec_parameters_to_context(d->m_pAudioCodecCtx, d->m_pFormatCtx->streams[d->m_audioIndex]->codecpar);
            avcodec_open2(d->m_pAudioCodecCtx, d->m_pAudioCodec, nullptr);
        }

        return 0;
    }

    int DecoderVAAPI::deinit() {
        Q_D(AVQt::DecoderVAAPI);
        avformat_close_input(&d->m_pFormatCtx);
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
            d->m_avfCallbacks.append(frameSink);
            result = static_cast<int>(d->m_avfCallbacks.indexOf(frameSink));
        }
        if (type & CB_QIMAGE) {
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
        int count = d->m_avfCallbacks.removeAll(frameSink);
        return count + static_cast<int>(d->m_qiCallbacks.removeAll(frameSink));
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
                AVPacket *packet = av_packet_alloc();
                AVFrame *hwFrame = av_frame_alloc();

                int ret = av_read_frame(d->m_pFormatCtx, packet);
                if (ret < 0) {
                    av_packet_free(&packet);
                    qWarning() << "Error while getting packet from AVFormatContext:" << av_make_error_string(new char[32], 32, ret);
                    return;
                }

                if (packet->stream_index == d->m_videoIndex) {
                    ret = avcodec_send_packet(d->m_pVideoCodecCtx, packet);
                    if (ret < 0) {
                        av_packet_free(&packet);
                        qWarning() << "Error sending packet to decoder:" << av_make_error_string(new char[128], 128, ret);
                        return;
                    }
                    while (true) {
                        auto t1 = NOW();
                        if (!(hwFrame = av_frame_alloc()) || !(d->m_pCurrentFrame = av_frame_alloc())) {
                            av_packet_free(&packet);
                            qWarning() << "Error allocating frames";
                            return;
                        }

                        ret = avcodec_receive_frame(d->m_pVideoCodecCtx, hwFrame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            av_frame_free(&hwFrame);
//                            av_freep(hwFrame->data[0]);
                            av_frame_free(&d->m_pCurrentFrame);
                            break;
                        } else if (ret < 0) {
                            av_packet_free(&packet);
                            qWarning() << "Error receiving frame from decoder:" << av_make_error_string(new char[128], 128, ret);
                            return;
                        }

//                        char strbuf[32];
//                        qDebug("Frame pix fmt: %s", av_get_pix_fmt_string(strbuf, 32, static_cast<AVPixelFormat>(hwFrame->format)));

                        if (hwFrame->format == AV_PIX_FMT_VAAPI) {
//                            ret = av_hwframe_map(d->m_pCurrentFrame, hwFrame, 0);
                            ret = av_hwframe_transfer_data(d->m_pCurrentFrame, hwFrame, 0);
                            if (ret < 0) {
                                qFatal("%d: Error mapping frame from gpu: %s", ret, av_make_error_string(new char[128], 128, ret));
                            }
//                            qDebug() << hwFrame->hw_frames_ctx->data;
                        } else {
                            qDebug() << "No hw frame";
                            av_frame_free(&d->m_pCurrentFrame);
                            d->m_pCurrentFrame = hwFrame;
                        }

//                        char strbuf[32];
//                        qDebug() << "                " << av_get_pix_fmt_string(strbuf, 32, AV_PIX_FMT_NONE);
//                        qDebug() << "Sw Frame format:"
//                                 << av_get_pix_fmt_string(strbuf, 32, static_cast<AVPixelFormat>(d->m_pCurrentFrame->format));

                        if (!d->m_qiCallbacks.isEmpty()) {
                            if (!d->m_pSwsContext) {
                                d->m_pSwsContext = sws_getContext(d->m_pCurrentFrame->width, d->m_pCurrentFrame->height,
                                                                  static_cast<AVPixelFormat>(d->m_pCurrentFrame->format),
                                                                  d->m_pCurrentFrame->width,
                                                                  d->m_pCurrentFrame->height, AV_PIX_FMT_BGRA, 0, nullptr, nullptr,
                                                                  nullptr);
                            }
                            d->m_pCurrentBGRAFrame = av_frame_alloc();

                            d->m_pCurrentBGRAFrame->width = hwFrame->width;
                            d->m_pCurrentBGRAFrame->height = hwFrame->height;
                            d->m_pCurrentBGRAFrame->format = AV_PIX_FMT_BGRA;

                            av_image_alloc(d->m_pCurrentBGRAFrame->data, d->m_pCurrentBGRAFrame->linesize, d->m_pCurrentFrame->width,
                                           d->m_pCurrentFrame->height, AV_PIX_FMT_BGRA, 1);

//                            qDebug() << "Scaling...";
                            sws_scale(d->m_pSwsContext, d->m_pCurrentFrame->data, d->m_pCurrentFrame->linesize, 0,
                                      d->m_pCurrentFrame->height,
                                      d->m_pCurrentBGRAFrame->data,
                                      d->m_pCurrentBGRAFrame->linesize);
                            QImage frame(d->m_pCurrentBGRAFrame->data[0], d->m_pCurrentFrame->width, d->m_pCurrentFrame->height,
                                         QImage::Format_ARGB32);

                            for (const auto &cb: d->m_qiCallbacks) {
//                                QtConcurrent::run([&](const QImage &image) {
                                cb->onFrame(frame.copy(), d->m_pFormatCtx->streams[d->m_videoIndex]->time_base,
                                            d->m_pFormatCtx->streams[d->m_videoIndex]->avg_frame_rate, d->m_pCurrentFrame->pkt_duration);
//                                }, frame.copy());
                            }
                            av_freep(d->m_pCurrentBGRAFrame->data);
                            av_frame_free(&d->m_pCurrentBGRAFrame);
                            d->m_pCurrentBGRAFrame = nullptr;
                        }

                        if (!d->m_avfCallbacks.isEmpty()) {
                            for (const auto &cb: d->m_avfCallbacks) {
                                AVFrame *cbFrame = av_frame_alloc();
                                av_frame_ref(cbFrame, d->m_pCurrentFrame);
//                        cbFrame->hw_frames_ctx = av_buffer_ref(hwFrame->hw_frames_ctx);
//                        av_image_alloc(cbFrame->data, cbFrame->linesize, d->m_pCurrentFrame->width, d->m_pCurrentFrame->height,
//                                       static_cast<AVPixelFormat>(d->m_pCurrentFrame->format), 1);
//                        av_frame_copy_props(cbFrame, d->m_pCurrentFrame);
//                        av_frame_copy(cbFrame, d->m_pCurrentFrame);
                                cbFrame->format = d->m_pCurrentFrame->format;
                                cbFrame->width = d->m_pCurrentFrame->width;
                                cbFrame->height = d->m_pCurrentFrame->height;
                                cbFrame->pkt_dts = hwFrame->pkt_dts;
                                cbFrame->pkt_size = hwFrame->pkt_size;
                                cbFrame->pkt_duration = hwFrame->pkt_duration;
                                cbFrame->pkt_pos = hwFrame->pkt_pos;
                                cbFrame->pts = hwFrame->pts;
//                                QtConcurrent::run([&](AVFrame *avFrame) {
                                auto t3 = NOW();
                                cb->onFrame(cbFrame, d->m_pFormatCtx->streams[d->m_videoIndex]->time_base,
                                            d->m_pFormatCtx->streams[d->m_videoIndex]->avg_frame_rate,
                                            cbFrame->pkt_duration * av_q2d(d->m_pFormatCtx->streams[d->m_videoIndex]->time_base) * 1000.0);
//                                }, cbFrame);
                                auto t4 = NOW();
                                qDebug("CB time: %ld us", TIME_US(t3, t4));
                                av_frame_unref(cbFrame);
                            }
                        }

                        auto t2 = NOW();
                        auto duration = hwFrame->pkt_duration * 1000000.0 *
                                        av_q2d(d->m_pFormatCtx->streams[packet->stream_index]->time_base) - TIME_US(t1, t2);
                        duration = (duration < 0 ? 0 : duration);
                        qDebug("Decoder: Frametime %ld us; Sleeping: %f us", TIME_US(t1, t2), duration);
                        usleep(duration);

                        av_frame_free(&hwFrame);
                        av_frame_free(&d->m_pCurrentFrame);
                        d->m_pCurrentFrame = nullptr;
                        hwFrame = nullptr;
                    }
                } else if (packet->stream_index == d->m_audioIndex) {
                    qDebug() << "Audio processing not implemented, discarding packet ...";
                }
                av_packet_free(&packet);
            } else {
                msleep(2);
            }
        }
    }
}