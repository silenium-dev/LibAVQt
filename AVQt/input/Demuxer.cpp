//
// Created by silas on 3/25/21.
//

#include <iostream>
#include "Demuxer.h"
#include "private/Demuxer_p.h"
#include "../IPacketSink.h"


namespace AVQt {
    [[maybe_unused]] Demuxer::Demuxer(QIODevice *inputDevice, QObject *parent) : QThread(parent), d_ptr(new DemuxerPrivate(this)) {
        Q_D(AVQt::Demuxer);

        d->m_inputDevice = inputDevice;
    }

    Demuxer::Demuxer(DemuxerPrivate &p) : d_ptr(&p) {
        Q_D(AVQt::Demuxer);

    }

    void Demuxer::pause(bool pause) {
        Q_D(AVQt::Demuxer);

        bool pauseFlag = !pause;
        d->m_paused.compare_exchange_strong(pauseFlag, pause);
        if (pauseFlag != pause) {
            d->m_paused.store(pause);
        }
    }

    bool Demuxer::isPaused() {
        Q_D(AVQt::Demuxer);
        return d->m_paused.load();
    }

    qsizetype Demuxer::registerCallback(IPacketSink *packetSink, uint8_t type) {
        Q_D(AVQt::Demuxer);

        QMutexLocker lock(&d->m_cbMutex);
        if (!d->m_cbMap.contains(packetSink)) {
            d->m_cbMap.insert(packetSink, type);
        }
        lock.unlock();

        if (d->m_initialized.load()) {
            AVCodecParameters *vParams = nullptr;
            AVCodecParameters *aParams = nullptr;
            AVCodecParameters *sParams = nullptr;
            if (type & CB_VIDEO) {
                vParams = avcodec_parameters_alloc();
                avcodec_parameters_copy(vParams, d->m_pFormatCtx->streams[d->m_videoStream]->codecpar);
            }
            if (type & CB_AUDIO) {
                aParams = avcodec_parameters_alloc();
                avcodec_parameters_copy(aParams, d->m_pFormatCtx->streams[d->m_subtitleStream]->codecpar);
            }
            if (type & CB_SUBTITLE) {
                sParams = avcodec_parameters_alloc();
                avcodec_parameters_copy(sParams, d->m_pFormatCtx->streams[d->m_subtitleStream]->codecpar);
            }
            packetSink->init(this, d->m_pFormatCtx->streams[d->m_videoStream]->avg_frame_rate, AVRational(),
                             d->m_pFormatCtx->duration * 1000.0 / AV_TIME_BASE, vParams, aParams, sParams);
            if (vParams) {
                avcodec_parameters_free(&vParams);
            }
            if (aParams) {
                avcodec_parameters_free(&aParams);
            }
            if (sParams) {
                avcodec_parameters_free(&sParams);
            }
        }
        if (d->m_running.load()) {
            packetSink->start(this);
        }

        return 0;
    }

    qsizetype Demuxer::unregisterCallback(IPacketSink *packetSink) {
        Q_D(AVQt::Demuxer);

        QMutexLocker lock(&d->m_cbMutex);
        if (d->m_cbMap.contains(packetSink)) {
            d->m_cbMap.remove(packetSink);

            if (d->m_running.load()) {
                packetSink->stop(this);
            }
            if (d->m_initialized.load()) {
                packetSink->deinit(this);
            }

            return 0;
        } else {
            return -1;
        }
    }

    int Demuxer::init() {
        Q_D(AVQt::Demuxer);
        bool initialized = true;
        d->m_initialized.compare_exchange_strong(initialized, true);
        if (!initialized) {
            d->m_pBuffer = new uint8_t[DemuxerPrivate::BUFFER_SIZE];
            d->m_pIOCtx = avio_alloc_context(d->m_pBuffer, DemuxerPrivate::BUFFER_SIZE, 0, d->m_inputDevice,
                                             &DemuxerPrivate::readFromIO, nullptr, &DemuxerPrivate::seekIO);
            d->m_pFormatCtx = avformat_alloc_context();
            d->m_pFormatCtx->pb = d->m_pIOCtx;
            d->m_pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

            if (avformat_open_input(&d->m_pFormatCtx, "", nullptr, nullptr) < 0) {
                qFatal("Could not open input format context");
            }

            if (d->m_pFormatCtx->iformat == av_find_input_format("mpegts") || d->m_pFormatCtx->iformat == av_find_input_format("rtp")) {
                avformat_find_stream_info(d->m_pFormatCtx, nullptr);
            }

            for (size_t si = 0; si < d->m_pFormatCtx->nb_streams; ++si) {
                switch (d->m_pFormatCtx->streams[si]->codecpar->codec_type) {
                    case AVMEDIA_TYPE_VIDEO:
                        d->m_videoStreams.append(si);
                        if (d->m_videoStream == -1) {
                            d->m_videoStream = si;
                        }
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                        d->m_audioStreams.append(si);
                        if (d->m_audioStream == -1) {
                            d->m_audioStream = si;
                        }
                        break;
                    case AVMEDIA_TYPE_SUBTITLE:
                        d->m_subtitleStreams.append(si);
                        if (d->m_subtitleStream == -1) {
                            d->m_subtitleStream = si;
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        for (const auto &cb: d->m_cbMap.keys()) {
            AVCodecParameters *vParams = nullptr;
            AVCodecParameters *aParams = nullptr;
            AVCodecParameters *sParams = nullptr;
            if (d->m_cbMap[cb] & CB_VIDEO && d->m_videoStream >= 0) {
                vParams = avcodec_parameters_alloc();
                avcodec_parameters_copy(vParams, d->m_pFormatCtx->streams[d->m_videoStream]->codecpar);
            }
            if (d->m_cbMap[cb] & CB_AUDIO && d->m_audioStream >= 0) {
                aParams = avcodec_parameters_alloc();
                avcodec_parameters_copy(aParams, d->m_pFormatCtx->streams[d->m_audioStream]->codecpar);
            }
            if (d->m_cbMap[cb] & CB_SUBTITLE && d->m_subtitleStream >= 0) {
                sParams = avcodec_parameters_alloc();
                avcodec_parameters_copy(sParams, d->m_pFormatCtx->streams[d->m_subtitleStream]->codecpar);
            }
            if (vParams) {
                cb->init(this, d->m_pFormatCtx->streams[d->m_videoStream]->avg_frame_rate,
                         d->m_pFormatCtx->streams[d->m_videoStream]->time_base,
                         d->m_pFormatCtx->duration * 1000.0 / AV_TIME_BASE, vParams, nullptr, nullptr);
            }
            if (aParams) {
                cb->init(this, d->m_pFormatCtx->streams[d->m_audioStream]->avg_frame_rate,
                         d->m_pFormatCtx->streams[d->m_audioStream]->time_base,
                         d->m_pFormatCtx->duration * 1000.0 / AV_TIME_BASE, nullptr, aParams, nullptr);
            }
            if (sParams) {
                cb->init(this, d->m_pFormatCtx->streams[d->m_subtitleStream]->avg_frame_rate,
                         d->m_pFormatCtx->streams[d->m_subtitleStream]->time_base,
                         d->m_pFormatCtx->duration * 1000.0 / AV_TIME_BASE, nullptr, nullptr, sParams);
            }
            if (vParams) {
                avcodec_parameters_free(&vParams);
            }
            if (aParams) {
                avcodec_parameters_free(&aParams);
            }
            if (sParams) {
                avcodec_parameters_free(&sParams);
            }
        }

        return 0;
    }

    int Demuxer::deinit() {
        Q_D(AVQt::Demuxer);

        stop();

        for (const auto &cb: d->m_cbMap.keys()) {
            cb->deinit(this);
        }

//        avio_closep(&d->m_pIOCtx);
        avformat_close_input(&d->m_pFormatCtx);
        delete[] d->m_pBuffer;

        d->m_videoStreams.clear();
        d->m_audioStreams.clear();
        d->m_subtitleStreams.clear();
        d->m_videoStream = -1;
        d->m_audioStream = -1;
        d->m_subtitleStream = -1;

        return 0;
    }

    int Demuxer::start() {
        Q_D(AVQt::Demuxer);
        bool notRunning = false;
        if (d->m_running.compare_exchange_strong(notRunning, true)) {
            d->m_paused.store(false);
            paused(false);

            for (const auto &cb: d->m_cbMap.keys()) {
                cb->start(this);
            }

            QThread::start();
            started();
            return 0;
        }
        return -1;
    }

    int Demuxer::stop() {
        Q_D(AVQt::Demuxer);
        bool running = true;
        if (d->m_running.compare_exchange_strong(running, false)) {
            for (const auto &cb: d->m_cbMap.keys()) {
                cb->stop(this);
            }

            wait();

            pause(true);
            stopped();
            return 0;
        }
        return -1;
    }

    void Demuxer::run() {
        Q_D(AVQt::Demuxer);

        int ret;
        constexpr size_t strBufSize = 64;
        char strBuf[strBufSize];

        AVPacket *packet = av_packet_alloc();
        QElapsedTimer elapsedTimer;
        size_t videoPackets = 0, audioPackets = 0, sttPackets = 0, packetCount = 0;
        elapsedTimer.start();
        while (d->m_running.load()) {
            if (!d->m_paused.load()) {
                ret = av_read_frame(d->m_pFormatCtx, packet);
                if (ret == AVERROR_EOF) {
                    break;
                } else if (ret != 0) {
                    qFatal("%d: Could not read packet from input %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }

                QList<IPacketSink *> cbList;
                CB_TYPE type;

                ++packetCount;

                if (packet->stream_index == d->m_videoStream) {
                    cbList = d->m_cbMap.keys(CB_VIDEO);
                    ++videoPackets;
                    type = CB_VIDEO;
                } else if (packet->stream_index == d->m_audioStream) {
                    cbList = d->m_cbMap.keys(CB_AUDIO);
                    qDebug("Audio packet duration: %f ms",
                           packet->duration * 1000.0 * av_q2d(d->m_pFormatCtx->streams[d->m_audioStream]->time_base));
                    ++audioPackets;
                    type = CB_AUDIO;
                } else if (packet->stream_index == d->m_subtitleStream) {
                    cbList = d->m_cbMap.keys(CB_SUBTITLE);
                    ++sttPackets;
                    type = CB_SUBTITLE;
                } else {
                    av_packet_unref(packet);
                    continue;
                }
                for (const auto &cb: cbList) {
                    AVPacket *cbPacket = av_packet_clone(packet);
                    cb->onPacket(this, cbPacket, type);
                    av_packet_unref(cbPacket);
                }
                if (elapsedTimer.hasExpired(500)) {
                    QByteArray aP, vP, sP, aR, vR, sR;
                    aP = QString("%1").arg(audioPackets, 12).toLocal8Bit();
                    vP = QString("%1").arg(videoPackets, 12).toLocal8Bit();
                    sP = QString("%1").arg(sttPackets, 12).toLocal8Bit();
                    aR = QString("%1").arg((audioPackets * 1.0 / packetCount) * 100.0, 10).toLocal8Bit();
                    vR = QString("%1").arg((videoPackets * 1.0 / packetCount) * 100.0, 10).toLocal8Bit();
                    sR = QString("%1").arg((sttPackets * 1.0 / packetCount) * 100.0, 10).toLocal8Bit();

                    qDebug() << "Packet statistics";
                    qDebug() << "| Packet type | Packet count |   Percentage |";
                    qDebug() << "| Video       |" << vP.data() << "|" << vR.data() << "% |";
                    qDebug() << "| Audio       |" << aP.data() << "|" << aR.data() << "% |";
                    qDebug() << "| Subtitle    |" << sP.data() << "|" << sR.data() << "% |";
                    qDebug();
                    elapsedTimer.restart();
                }
            } else {
                msleep(8);
            }
        }
    }

    Demuxer::Demuxer(Demuxer &&other) {
        d_ptr = other.d_ptr;
        d_ptr->q_ptr = this;
        other.d_ptr = nullptr;
    }

    Demuxer &Demuxer::operator=(Demuxer &&other) noexcept {
        delete d_ptr;
        d_ptr = other.d_ptr;
        d_ptr->q_ptr = this;
        other.d_ptr = nullptr;

        return *this;
    }

    int DemuxerPrivate::readFromIO(void *opaque, uint8_t *buf, int bufSize) {
        auto *inputDevice = reinterpret_cast<QIODevice *>(opaque);

        auto bytesRead = inputDevice->read(reinterpret_cast<char *>(buf), bufSize);
        if (bytesRead == 0) {
            return AVERROR_EOF;
        } else {
            return bytesRead;
        }
    }

    int64_t DemuxerPrivate::seekIO(void *opaque, int64_t pos, int whence) {
        auto *inputDevice = reinterpret_cast<QIODevice *>(opaque);

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
}