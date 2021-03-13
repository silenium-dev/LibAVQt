//
// Created by silas on 3/1/21.
//

#include "DecoderVAAPI.h"

#include <QImage>
#include <QtConcurrent>

namespace AV {

    DecoderVAAPI::DecoderVAAPI(QIODevice *inputDevice, QObject *parent) : QThread(parent), m_inputDevice(inputDevice) {
//        av_log_set_level(AV_LOG_DEBUG);
        connect(this, &QThread::finished, [&] {
            qApp->quit();
        });
    }

    int DecoderVAAPI::init() {
        qDebug() << "Initializing VAAPI decoder";

        pIOBuffer = static_cast<uint8_t *>(av_malloc(32 * 1024));

        pInputCtx = avio_alloc_context(pIOBuffer, 32 * 1024, 0, m_inputDevice, &DecoderVAAPI::readFromIO, nullptr, &DecoderVAAPI::seekIO);

        pFormatCtx = avformat_alloc_context();

        pFormatCtx->pb = pInputCtx;

        pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

        if (avformat_open_input(&pFormatCtx, "", nullptr, nullptr) < 0) {
            qDebug() << "Error opening format context";
            return -1;
        }

//        int videoIndex = -1, audioIndex = -1;

        for (int i = 0; i < pFormatCtx->nb_streams; ++i) {
            if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoIndex = i;
                if (audioIndex >= 0) {
                    break;
                } else {
                    continue;
                }
            }
            if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioIndex = i;
                if (videoIndex >= 0) {
                    break;
                } else {
                    continue;
                }
            }
        }

        if (videoIndex >= 0) {
            pVideoCodec = avcodec_find_decoder(pFormatCtx->streams[videoIndex]->codecpar->codec_id);
            pVideoCodecCtx = avcodec_alloc_context3(pVideoCodec);
            avcodec_parameters_to_context(pVideoCodecCtx, pFormatCtx->streams[videoIndex]->codecpar);

            av_hwdevice_ctx_create(&pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
//            pFramesCtx = av_hwframe_ctx_alloc(pDeviceCtx);
//            av_hwframe_ctx_init(pFramesCtx);
            pVideoCodecCtx->hw_device_ctx = pDeviceCtx;
//            pVideoCodecCtx->hw_frames_ctx = pFramesCtx;
//            auto hwconfig = avcodec_get_hw_config(pVideoCodec, 0);
//            auto hwconstrains = av_hwdevice_get_hwframe_constraints(pDeviceCtx, hwconfig);

            avcodec_open2(pVideoCodecCtx, pVideoCodec, nullptr);
        }

        if (audioIndex >= 0) {
            pAudioCodec = avcodec_find_decoder(pFormatCtx->streams[audioIndex]->codecpar->codec_id);
            pAudioCodecCtx = avcodec_alloc_context3(pAudioCodec);
            avcodec_parameters_to_context(pAudioCodecCtx, pFormatCtx->streams[audioIndex]->codecpar);
            avcodec_open2(pAudioCodecCtx, pAudioCodec, nullptr);
        }


        return 0;
    }

    int DecoderVAAPI::deinit() {
        avformat_close_input(&pFormatCtx);
        avio_context_free(&pInputCtx);
        return 0;
    }

    int DecoderVAAPI::start() {
        if (!isRunning) {
            isRunning.store(true);
            QThread::start(QThread::TimeCriticalPriority);
            return 0;
        } else {
            return -1;
        }
    }

    int DecoderVAAPI::stop() {
        if (isRunning) {
            isRunning.store(false);
            while (!isFinished()) {
                QThread::msleep(1);
            }
            return 0;
        } else {
            return -1;
        }
    }

    int DecoderVAAPI::pause(bool pause) {
        if (isRunning) {
            isPaused.store(pause);
            return 0;
        } else {
            return -1;
        }
    }

    int DecoderVAAPI::registerCallback(IFrameSink *frameSink, uint8_t type) {
        int result = -1;
        if (type & CB_AVFRAME) {
            m_avfCallbacks.append(frameSink);
            result = static_cast<int>(m_avfCallbacks.indexOf(frameSink));
        }
        if (type & CB_QIMAGE) {
            if (result == -1) {
                result = 0;
            }
            m_qiCallbacks.append(frameSink);
            result += static_cast<int>((m_qiCallbacks.indexOf(frameSink) & 0xFF) << 8);
        }
        return result;
    }

    int DecoderVAAPI::unregisterCallback(IFrameSink *frameSink) {
        int count = m_avfCallbacks.removeAll(frameSink);
        return count + static_cast<int>(m_qiCallbacks.removeAll(frameSink));
    }

    int DecoderVAAPI::readFromIO(void *opaque, uint8_t *buf, int bufSize) {
        auto *inputDevice = static_cast<QIODevice *>(opaque);

        auto bytesRead = inputDevice->read(reinterpret_cast<char *>(buf), bufSize);
        if (bytesRead == 0) {
            return AVERROR_EOF;
        } else {
            return bytesRead;
        }
    }

    int64_t DecoderVAAPI::seekIO(void *opaque, int64_t pos, int whence) {
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
        while (isRunning.load()) {
            if (!isPaused.load()) {
                AVPacket *packet = av_packet_alloc();
                AVFrame *hwFrame = av_frame_alloc();

                int ret = av_read_frame(pFormatCtx, packet);
                if (ret < 0) {
                    av_packet_free(&packet);
                    qWarning() << "Error while getting packet from AVFormatContext:" << av_make_error_string(new char[32], 32, ret);
                    return;
                }

                if (packet->stream_index == videoIndex) {
                    ret = avcodec_send_packet(pVideoCodecCtx, packet);
                    if (ret < 0) {
                        av_packet_free(&packet);
                        qWarning() << "Error sending packet to decoder:" << av_make_error_string(new char[128], 128, ret);
                        return;
                    }
                    while (true) {
                        if (!(hwFrame = av_frame_alloc()) || !(pCurrentFrame = av_frame_alloc())) {
                            av_packet_free(&packet);
                            qWarning() << "Error allocating frames";
                            return;
                        }

                        ret = avcodec_receive_frame(pVideoCodecCtx, hwFrame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            av_frame_free(&hwFrame);
//                            av_freep(hwFrame->data[0]);
                            av_frame_free(&pCurrentFrame);
                            break;
                        } else if (ret < 0) {
                            av_packet_free(&packet);
                            qWarning() << "Error receiving frame from decoder:" << av_make_error_string(new char[128], 128, ret);
                            return;
                        }

//                        char strbuf[32];
//                        qDebug("Frame pix fmt: %s", av_get_pix_fmt_string(strbuf, 32, static_cast<AVPixelFormat>(hwFrame->format)));

                        if (hwFrame->format == AV_PIX_FMT_VAAPI) {
//                            ret = av_hwframe_map(pCurrentFrame, hwFrame, 0);
                            ret = av_hwframe_transfer_data(pCurrentFrame, hwFrame, 0);
                            if (ret < 0) {
                                qFatal("%d: Error mapping frame from gpu: %s", ret, av_make_error_string(new char[128], 128, ret));
                            }
//                            qDebug() << hwFrame->hw_frames_ctx->data;
                        } else {
                            qDebug() << "No hw frame";
                            av_frame_free(&pCurrentFrame);
                            pCurrentFrame = hwFrame;
                        }

//                        char strbuf[32];
//                        qDebug() << "                " << av_get_pix_fmt_string(strbuf, 32, AV_PIX_FMT_NONE);
//                        qDebug() << "Sw Frame format:"
//                                 << av_get_pix_fmt_string(strbuf, 32, static_cast<AVPixelFormat>(pCurrentFrame->format));

                        if (!pSwsContext) {
                            pSwsContext = sws_getContext(pCurrentFrame->width, pCurrentFrame->height,
                                                         static_cast<AVPixelFormat>(pCurrentFrame->format), pCurrentFrame->width,
                                                         pCurrentFrame->height, AV_PIX_FMT_BGRA, 0, nullptr, nullptr, nullptr);
                        }


                        if (!m_qiCallbacks.isEmpty()) {
                            pCurrentBGRAFrame = av_frame_alloc();

                            pCurrentBGRAFrame->width = hwFrame->width;
                            pCurrentBGRAFrame->height = hwFrame->height;
                            pCurrentBGRAFrame->format = AV_PIX_FMT_BGRA;

                            av_image_alloc(pCurrentBGRAFrame->data, pCurrentBGRAFrame->linesize, pCurrentFrame->width,
                                           pCurrentFrame->height, AV_PIX_FMT_BGRA, 1);

                            qDebug() << "Scaling...";
                            sws_scale(pSwsContext, pCurrentFrame->data, pCurrentFrame->linesize, 0, pCurrentFrame->height,
                                      pCurrentBGRAFrame->data,
                                      pCurrentBGRAFrame->linesize);
                            QImage frame(pCurrentBGRAFrame->data[0], pCurrentFrame->width, pCurrentFrame->height, QImage::Format_ARGB32);

                            for (const auto &cb: m_qiCallbacks) {
//                                QtConcurrent::run([&](const QImage &image) {
                                cb->onFrame(frame.copy(), pVideoCodecCtx->time_base, pVideoCodecCtx->framerate);
//                                }, frame.copy());
                            }
                            av_freep(pCurrentBGRAFrame->data);
                            av_frame_free(&pCurrentBGRAFrame);
                            pCurrentBGRAFrame = nullptr;
                        }

                        if (!m_avfCallbacks.isEmpty()) {
                            for (const auto &cb: m_avfCallbacks) {
                                AVFrame *cbFrame = av_frame_alloc();
                                av_frame_ref(cbFrame, pCurrentFrame);
//                        cbFrame->hw_frames_ctx = av_buffer_ref(hwFrame->hw_frames_ctx);
//                        av_image_alloc(cbFrame->data, cbFrame->linesize, pCurrentFrame->width, pCurrentFrame->height,
//                                       static_cast<AVPixelFormat>(pCurrentFrame->format), 1);
//                        av_frame_copy_props(cbFrame, pCurrentFrame);
//                        av_frame_copy(cbFrame, pCurrentFrame);
                                cbFrame->format = pCurrentFrame->format;
                                cbFrame->width = pCurrentFrame->width;
                                cbFrame->height = pCurrentFrame->height;
                                cbFrame->pkt_dts = hwFrame->pkt_dts;
                                cbFrame->pkt_size = hwFrame->pkt_size;
                                cbFrame->pkt_duration = hwFrame->pkt_duration;
                                cbFrame->pkt_pos = hwFrame->pkt_pos;
                                cbFrame->pts = hwFrame->pts;
//                                QtConcurrent::run([&](AVFrame *avFrame) {
                                cb->onFrame(cbFrame, pVideoCodecCtx->time_base, pVideoCodecCtx->framerate);
//                                }, cbFrame);
                                av_frame_unref(cbFrame);
                            }
                        }

                        av_frame_free(&hwFrame);
                        av_frame_free(&pCurrentFrame);
                        pCurrentFrame = nullptr;
                        hwFrame = nullptr;
                    }
                } else if (packet->stream_index == audioIndex) {
                    qDebug() << "Audio processing not implemented, discarding packet ...";
                }
                av_packet_free(&packet);
            } else {
                msleep(2);
            }
        }
    }
}