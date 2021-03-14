//
// Created by silas on 3/1/21.
//

#include "private/DecoderVAAPI_p.h"
#include "DecoderVAAPI.h"
#include "../output/IFrameSink.h"

#include <QImage>

namespace AV {

    DecoderVAAPI::DecoderVAAPI(QIODevice *inputDevice, QObject *parent) : QThread(parent), d_ptr(new DecoderVAAPIPrivate(this)) {
        Q_D(AV::DecoderVAAPI);

        d->m_inputDevice = inputDevice;

        connect(this, &QThread::finished, [&] {
            QCoreApplication::quit();
        });
    }

    DecoderVAAPI::DecoderVAAPI(DecoderVAAPIPrivate &p) : d_ptr(&p) {

    }

    int DecoderVAAPI::init() {
        Q_D(AV::DecoderVAAPI);
        qDebug() << "Initializing VAAPI decoder";

        d->pIOBuffer = static_cast<uint8_t *>(av_malloc(32 * 1024));

        d->pInputCtx = avio_alloc_context(d->pIOBuffer, 32 * 1024, 0, d->m_inputDevice, &DecoderVAAPIPrivate::readFromIO, nullptr,
                                          &DecoderVAAPIPrivate::seekIO);

        d->pFormatCtx = avformat_alloc_context();

        d->pFormatCtx->pb = d->pInputCtx;

        d->pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

        if (avformat_open_input(&d->pFormatCtx, "", nullptr, nullptr) < 0) {
            qDebug() << "Error opening format context";
            return -1;
        }

//        int videoIndex = -1, d->audioIndex = -1;

        for (int i = 0; i < d->pFormatCtx->nb_streams; ++i) {
            if (d->pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                d->videoIndex = i;
                if (d->audioIndex >= 0) {
                    break;
                } else {
                    continue;
                }
            }
            if (d->pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                d->audioIndex = i;
                if (d->videoIndex >= 0) {
                    break;
                } else {
                    continue;
                }
            }
        }

        if (d->videoIndex >= 0) {
            d->pVideoCodec = avcodec_find_decoder(d->pFormatCtx->streams[d->videoIndex]->codecpar->codec_id);
            d->pVideoCodecCtx = avcodec_alloc_context3(d->pVideoCodec);
            avcodec_parameters_to_context(d->pVideoCodecCtx, d->pFormatCtx->streams[d->videoIndex]->codecpar);

            av_hwdevice_ctx_create(&d->pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
//            pFramesCtx = av_hwframe_ctx_alloc(d->pDeviceCtx);
//            av_hwframe_ctx_init(pFramesCtx);
            d->pVideoCodecCtx->hw_device_ctx = d->pDeviceCtx;
//            d->pVideoCodecCtx->hw_frames_ctx = pFramesCtx;
//            auto hwconfig = avcodec_get_hw_config(d->pVideoCodec, 0);
//            auto hwconstrains = av_hwdevice_get_hwframe_constraints(d->pDeviceCtx, hwconfig);

            avcodec_open2(d->pVideoCodecCtx, d->pVideoCodec, nullptr);
        }

        if (d->audioIndex >= 0) {
            d->pAudioCodec = avcodec_find_decoder(d->pFormatCtx->streams[d->audioIndex]->codecpar->codec_id);
            d->pAudioCodecCtx = avcodec_alloc_context3(d->pAudioCodec);
            avcodec_parameters_to_context(d->pAudioCodecCtx, d->pFormatCtx->streams[d->audioIndex]->codecpar);
            avcodec_open2(d->pAudioCodecCtx, d->pAudioCodec, nullptr);
        }


        return 0;
    }

    int DecoderVAAPI::deinit() {
        Q_D(AV::DecoderVAAPI);
        avformat_close_input(&d->pFormatCtx);
        avio_context_free(&d->pInputCtx);
        return 0;
    }

    int DecoderVAAPI::start() {
        Q_D(AV::DecoderVAAPI);
        if (!d->isRunning) {
            d->isRunning.store(true);
            QThread::start(QThread::TimeCriticalPriority);
            return 0;
        } else {
            return -1;
        }
    }

    int DecoderVAAPI::stop() {
        Q_D(AV::DecoderVAAPI);
        if (d->isRunning) {
            d->isRunning.store(false);
            while (!isFinished()) {
                QThread::msleep(1);
            }
            return 0;
        } else {
            return -1;
        }
    }

    int DecoderVAAPI::pause(bool pause) {
        Q_D(AV::DecoderVAAPI);
        if (d->isRunning) {
            d->isPaused.store(pause);
            return 0;
        } else {
            return -1;
        }
    }

    int DecoderVAAPI::registerCallback(IFrameSink *frameSink, uint8_t type) {
        Q_D(AV::DecoderVAAPI);
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
        Q_D(AV::DecoderVAAPI);
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
        Q_D(AV::DecoderVAAPI);
        while (d->isRunning.load()) {
            if (!d->isPaused.load()) {
                AVPacket *packet = av_packet_alloc();
                AVFrame *hwFrame = av_frame_alloc();

                int ret = av_read_frame(d->pFormatCtx, packet);
                if (ret < 0) {
                    av_packet_free(&packet);
                    qWarning() << "Error while getting packet from AVFormatContext:" << av_make_error_string(new char[32], 32, ret);
                    return;
                }

                if (packet->stream_index == d->videoIndex) {
                    ret = avcodec_send_packet(d->pVideoCodecCtx, packet);
                    if (ret < 0) {
                        av_packet_free(&packet);
                        qWarning() << "Error sending packet to decoder:" << av_make_error_string(new char[128], 128, ret);
                        return;
                    }
                    while (true) {
                        if (!(hwFrame = av_frame_alloc()) || !(d->pCurrentFrame = av_frame_alloc())) {
                            av_packet_free(&packet);
                            qWarning() << "Error allocating frames";
                            return;
                        }

                        ret = avcodec_receive_frame(d->pVideoCodecCtx, hwFrame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            av_frame_free(&hwFrame);
//                            av_freep(hwFrame->data[0]);
                            av_frame_free(&d->pCurrentFrame);
                            break;
                        } else if (ret < 0) {
                            av_packet_free(&packet);
                            qWarning() << "Error receiving frame from decoder:" << av_make_error_string(new char[128], 128, ret);
                            return;
                        }

//                        char strbuf[32];
//                        qDebug("Frame pix fmt: %s", av_get_pix_fmt_string(strbuf, 32, static_cast<AVPixelFormat>(hwFrame->format)));

                        if (hwFrame->format == AV_PIX_FMT_VAAPI) {
//                            ret = av_hwframe_map(d->pCurrentFrame, hwFrame, 0);
                            ret = av_hwframe_transfer_data(d->pCurrentFrame, hwFrame, 0);
                            if (ret < 0) {
                                qFatal("%d: Error mapping frame from gpu: %s", ret, av_make_error_string(new char[128], 128, ret));
                            }
//                            qDebug() << hwFrame->hw_frames_ctx->data;
                        } else {
                            qDebug() << "No hw frame";
                            av_frame_free(&d->pCurrentFrame);
                            d->pCurrentFrame = hwFrame;
                        }

//                        char strbuf[32];
//                        qDebug() << "                " << av_get_pix_fmt_string(strbuf, 32, AV_PIX_FMT_NONE);
//                        qDebug() << "Sw Frame format:"
//                                 << av_get_pix_fmt_string(strbuf, 32, static_cast<AVPixelFormat>(d->pCurrentFrame->format));

                        if (!d->pSwsContext) {
                            d->pSwsContext = sws_getContext(d->pCurrentFrame->width, d->pCurrentFrame->height,
                                                            static_cast<AVPixelFormat>(d->pCurrentFrame->format), d->pCurrentFrame->width,
                                                            d->pCurrentFrame->height, AV_PIX_FMT_BGRA, 0, nullptr, nullptr, nullptr);
                        }


                        if (!d->m_qiCallbacks.isEmpty()) {
                            d->pCurrentBGRAFrame = av_frame_alloc();

                            d->pCurrentBGRAFrame->width = hwFrame->width;
                            d->pCurrentBGRAFrame->height = hwFrame->height;
                            d->pCurrentBGRAFrame->format = AV_PIX_FMT_BGRA;

                            av_image_alloc(d->pCurrentBGRAFrame->data, d->pCurrentBGRAFrame->linesize, d->pCurrentFrame->width,
                                           d->pCurrentFrame->height, AV_PIX_FMT_BGRA, 1);

                            qDebug() << "Scaling...";
                            sws_scale(d->pSwsContext, d->pCurrentFrame->data, d->pCurrentFrame->linesize, 0, d->pCurrentFrame->height,
                                      d->pCurrentBGRAFrame->data,
                                      d->pCurrentBGRAFrame->linesize);
                            QImage frame(d->pCurrentBGRAFrame->data[0], d->pCurrentFrame->width, d->pCurrentFrame->height,
                                         QImage::Format_ARGB32);

                            for (const auto &cb: d->m_qiCallbacks) {
//                                QtConcurrent::run([&](const QImage &image) {
                                cb->onFrame(frame.copy(), d->pVideoCodecCtx->time_base, d->pVideoCodecCtx->framerate);
//                                }, frame.copy());
                            }
                            av_freep(d->pCurrentBGRAFrame->data);
                            av_frame_free(&d->pCurrentBGRAFrame);
                            d->pCurrentBGRAFrame = nullptr;
                        }

                        if (!d->m_avfCallbacks.isEmpty()) {
                            for (const auto &cb: d->m_avfCallbacks) {
                                AVFrame *cbFrame = av_frame_alloc();
                                av_frame_ref(cbFrame, d->pCurrentFrame);
//                        cbFrame->hw_frames_ctx = av_buffer_ref(hwFrame->hw_frames_ctx);
//                        av_image_alloc(cbFrame->data, cbFrame->linesize, d->pCurrentFrame->width, d->pCurrentFrame->height,
//                                       static_cast<AVPixelFormat>(d->pCurrentFrame->format), 1);
//                        av_frame_copy_props(cbFrame, d->pCurrentFrame);
//                        av_frame_copy(cbFrame, d->pCurrentFrame);
                                cbFrame->format = d->pCurrentFrame->format;
                                cbFrame->width = d->pCurrentFrame->width;
                                cbFrame->height = d->pCurrentFrame->height;
                                cbFrame->pkt_dts = hwFrame->pkt_dts;
                                cbFrame->pkt_size = hwFrame->pkt_size;
                                cbFrame->pkt_duration = hwFrame->pkt_duration;
                                cbFrame->pkt_pos = hwFrame->pkt_pos;
                                cbFrame->pts = hwFrame->pts;
//                                QtConcurrent::run([&](AVFrame *avFrame) {
                                cb->onFrame(cbFrame, d->pVideoCodecCtx->time_base, d->pVideoCodecCtx->framerate);
//                                }, cbFrame);
                                av_frame_unref(cbFrame);
                            }
                        }

                        av_frame_free(&hwFrame);
                        av_frame_free(&d->pCurrentFrame);
                        d->pCurrentFrame = nullptr;
                        hwFrame = nullptr;
                    }
                } else if (packet->stream_index == d->audioIndex) {
                    qDebug() << "Audio processing not implemented, discarding packet ...";
                }
                av_packet_free(&packet);
            } else {
                msleep(2);
            }
        }
    }
}