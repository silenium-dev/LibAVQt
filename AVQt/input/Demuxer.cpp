#include "Demuxer.h"
#include "communication/Command.h"
#include "communication/PacketPadParams.h"
#include "output/IPacketSink.h"
#include "private/Demuxer_p.h"
#include <communication/CommandPadParams.h>

namespace AVQt {
    [[maybe_unused]] Demuxer::Demuxer(QIODevice *inputDevice, QObject *parent) : QThread(parent), d_ptr(new DemuxerPrivate(this)), ProcessingGraph::Producer(false) {
        Q_D(AVQt::Demuxer);

        d->m_inputDevice = inputDevice;
    }

    Demuxer::Demuxer(DemuxerPrivate &p) : d_ptr(&p) {
    }

    void Demuxer::pause(bool pause) {
        Q_D(AVQt::Demuxer);

        bool pauseFlag = !pause;
        if (d->m_paused.compare_exchange_strong(pauseFlag, pause)) {
            d->m_paused.store(pause);
            //            auto commandPtr = new Command(command);
            auto commandPads = d->m_commandPadIds.values();
            for (const auto &commandPadId : commandPads) {
                produce(Command::builder().withType(Command::Type::PAUSE).withPayload("state", pause).build(), commandPadId);
            }
            paused(pause);
        }
    }

    bool Demuxer::isPaused() {
        Q_D(AVQt::Demuxer);
        return d->m_paused.load();
    }

    int Demuxer::init() {
        Q_D(AVQt::Demuxer);
        bool shouldBe = false;
        if (d->m_initialized.compare_exchange_strong(shouldBe, true)) {
            d->m_pBuffer = static_cast<uint8_t *>(av_malloc(DemuxerPrivate::BUFFER_SIZE));
            d->m_pIOCtx = avio_alloc_context(d->m_pBuffer, DemuxerPrivate::BUFFER_SIZE, 0, d->m_inputDevice,
                                             &DemuxerPrivate::readFromIO, nullptr, &DemuxerPrivate::seekIO);
            d->m_pFormatCtx = avformat_alloc_context();
            d->m_pFormatCtx->pb = d->m_pIOCtx;
            d->m_pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

            if (avformat_open_input(&d->m_pFormatCtx, "", nullptr, nullptr) < 0) {
                qFatal("Could not open input format context");
            }

            //            if (d->m_pFormatCtx->iformat == av_find_input_format("mpegts") || d->m_pFormatCtx->iformat == av_find_input_format("rtp")) {
            avformat_find_stream_info(d->m_pFormatCtx, nullptr);
            //            }

            for (int64_t si = 0; si < d->m_pFormatCtx->nb_streams; ++si) {
                switch (d->m_pFormatCtx->streams[si]->codecpar->codec_type) {
                    case AVMEDIA_TYPE_VIDEO:
                        d->m_videoStreams.append(si);
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                        d->m_audioStreams.append(si);
                        break;
                    case AVMEDIA_TYPE_SUBTITLE:
                        d->m_subtitleStreams.append(si);
                        break;
                    default:
                        break;
                }
                PacketPadParams packetPadParams{};
                packetPadParams.mediaType = d->m_pFormatCtx->streams[si]->codecpar->codec_type;
                packetPadParams.codec = d->m_pFormatCtx->streams[si]->codecpar->codec_id;
                packetPadParams.stream = si;
                CommandPadParams commandPadParams{};
                commandPadParams.stream = si;
                d->m_streamPadIds.insert(si, createPad<AVPacket *>(QVariant::fromValue(packetPadParams), &AVQt::DemuxerPrivate::linkValidator));
                d->m_commandPadIds.insert(si, createPad<Command>(QVariant::fromValue(commandPadParams)));
                qDebug("Creating pad %ul", d->m_streamPadIds[si]);
            }
        }

        //        auto cbs = d->m_cbMap.keys();
        //        for (const auto &cb : cbs) {
        //            AVCodecParameters *vParams = nullptr;
        //            AVCodecParameters *aParams = nullptr;
        //            AVCodecParameters *sParams = nullptr;
        //            if (d->m_cbMap[cb] & CB_VIDEO && d->m_videoStream >= 0) {
        //                vParams = avcodec_parameters_alloc();
        //                avcodec_parameters_copy(vParams, d->m_pFormatCtx->streams[d->m_videoStream]->codecpar);
        //            }
        //            if (d->m_cbMap[cb] & CB_AUDIO && d->m_audioStream >= 0) {
        //                aParams = avcodec_parameters_alloc();
        //                avcodec_parameters_copy(aParams, d->m_pFormatCtx->streams[d->m_audioStream]->codecpar);
        //            }
        //            if (d->m_cbMap[cb] & CB_SUBTITLE && d->m_subtitleStream >= 0) {
        //                sParams = avcodec_parameters_alloc();
        //                avcodec_parameters_copy(sParams, d->m_pFormatCtx->streams[d->m_subtitleStream]->codecpar);
        //            }
        //            if (vParams) {
        //                cb->init(this, d->m_pFormatCtx->streams[d->m_videoStream]->avg_frame_rate,
        //                         d->m_pFormatCtx->streams[d->m_videoStream]->time_base,
        //                         static_cast<int64_t>(static_cast<double>(d->m_pFormatCtx->duration) * 1000.0 / AV_TIME_BASE), vParams, nullptr,
        //                         nullptr);
        //            }
        //            if (aParams) {
        //                cb->init(this, d->m_pFormatCtx->streams[d->m_audioStream]->avg_frame_rate,
        //                         d->m_pFormatCtx->streams[d->m_audioStream]->time_base,
        //                         static_cast<int64_t>(static_cast<double>(d->m_pFormatCtx->duration) * 1000.0 / AV_TIME_BASE), nullptr, aParams,
        //                         nullptr);
        //            }
        //            if (sParams) {
        //                cb->init(this, d->m_pFormatCtx->streams[d->m_subtitleStream]->avg_frame_rate,
        //                         d->m_pFormatCtx->streams[d->m_subtitleStream]->time_base,
        //                         static_cast<int64_t>(static_cast<double>(d->m_pFormatCtx->duration) * 1000.0 / AV_TIME_BASE), nullptr, nullptr,
        //                         sParams);
        //            }
        //            if (vParams) {
        //                avcodec_parameters_free(&vParams);
        //            }
        //            if (aParams) {
        //                avcodec_parameters_free(&aParams);
        //            }
        //            if (sParams) {
        //                avcodec_parameters_free(&sParams);
        //            }
        //        }

        return 0;
    }

    //    qint64 Demuxer::registerCallback(IPacketSink *packetSink, int8_t type) {
    //        Q_D(AVQt::Demuxer);
    //
    //        QMutexLocker lock(&d->m_cbMutex);
    //        if (!d->m_cbMap.contains(packetSink)) {
    //            d->m_cbMap.insert(packetSink, type);
    //        }
    //        lock.unlock();
    //
    //        if (d->m_initialized.load()) {
    //            AVCodecParameters *vParams = nullptr;
    //            AVCodecParameters *aParams = nullptr;
    //            AVCodecParameters *sParams = nullptr;
    //            if (type & CB_VIDEO) {
    //                vParams = avcodec_parameters_alloc();
    //                avcodec_parameters_copy(vParams, d->m_pFormatCtx->streams[d->m_videoStream]->codecpar);
    //            }
    //            if (type & CB_AUDIO) {
    //                aParams = avcodec_parameters_alloc();
    //                avcodec_parameters_copy(aParams, d->m_pFormatCtx->streams[d->m_subtitleStream]->codecpar);
    //            }
    //            if (type & CB_SUBTITLE) {
    //                sParams = avcodec_parameters_alloc();
    //                avcodec_parameters_copy(sParams, d->m_pFormatCtx->streams[d->m_subtitleStream]->codecpar);
    //            }
    //            packetSink->init(this, d->m_pFormatCtx->streams[d->m_videoStream]->avg_frame_rate, av_make_q(1, AV_TIME_BASE),
    //                             static_cast<int64_t>(static_cast<double>(d->m_pFormatCtx->duration) * 1000.0 / AV_TIME_BASE), vParams, aParams,
    //                             sParams);
    //            if (vParams) {
    //                avcodec_parameters_free(&vParams);
    //            }
    //            if (aParams) {
    //                avcodec_parameters_free(&aParams);
    //            }
    //            if (sParams) {
    //                avcodec_parameters_free(&sParams);
    //            }
    //        }
    //        if (d->m_running.load()) {
    //            packetSink->start(this);
    //        }
    //
    //        return 0;
    //    }
    //
    //    qint64 Demuxer::unregisterCallback(IPacketSink *packetSink) {
    //        Q_D(AVQt::Demuxer);
    //
    //        QMutexLocker lock(&d->m_cbMutex);
    //        if (d->m_cbMap.contains(packetSink)) {
    //            d->m_cbMap.remove(packetSink);
    //
    //            if (d->m_running.load()) {
    //                packetSink->stop(this);
    //            }
    //            if (d->m_initialized.load()) {
    //                packetSink->deinit(this);
    //            }
    //
    //            return 0;
    //        } else {
    //            return -1;
    //        }
    //    }
    //
    //
    //    int Demuxer::deinit() {
    //        Q_D(AVQt::Demuxer);
    //
    //        stop();
    //
    //        auto cbs = d->m_cbMap.keys();
    //        for (const auto &cb : cbs) {
    //            cb->deinit(this);
    //        }
    //
    //        //        avio_closep(&d->m_pIOCtx);
    //        avformat_close_input(&d->m_pFormatCtx);
    //
    //        d->m_videoStreams.clear();
    //        d->m_audioStreams.clear();
    //        d->m_subtitleStreams.clear();
    //        d->m_videoStream = -1;
    //        d->m_audioStream = -1;
    //        d->m_subtitleStream = -1;
    //
    //        return 0;
    //    }
    //
    //    int Demuxer::start() {
    //        Q_D(AVQt::Demuxer);
    //        bool notRunning = false;
    //        if (d->m_running.compare_exchange_strong(notRunning, true)) {
    //            d->m_paused.store(false);
    //            paused(false);
    //
    //            auto cbs = d->m_cbMap.keys();
    //            for (const auto &cb : cbs) {
    //                cb->start(this);
    //            }
    //
    //            QThread::start();
    //            started();
    //            return 0;
    //        }
    //        return -1;
    //    }
    //
    //    int Demuxer::stop() {
    //        Q_D(AVQt::Demuxer);
    //        bool running = true;
    //        if (d->m_running.compare_exchange_strong(running, false)) {
    //            auto cbs = d->m_cbMap.keys();
    //            for (const auto &cb : cbs) {
    //                cb->stop(this);
    //            }
    //
    //            wait();
    //
    //            stopped();
    //            return 0;
    //        }
    //        return -1;
    //    }
    //
    //    void Demuxer::run() {
    //        Q_D(AVQt::Demuxer);
    //
    //        int ret;
    //        constexpr size_t strBufSize = 64;
    //        char strBuf[strBufSize];
    //
    //        AVPacket *packet = av_packet_alloc();
    //        QElapsedTimer elapsedTimer;
    //        size_t videoPackets = 0, audioPackets = 0, sttPackets = 0, packetCount = 0;
    //        elapsedTimer.start();
    //        while (d->m_running.load()) {
    //            if (!d->m_paused.load()) {
    //                ret = av_read_frame(d->m_pFormatCtx, packet);
    //                if (ret == AVERROR_EOF) {
    //                    break;
    //                } else if (ret != 0) {
    //                    qFatal("%d: Could not read packet from input %s", ret, av_make_error_string(strBuf, strBufSize, ret));
    //                }
    //
    //                QList<IPacketSink *> cbList;
    //                CB_TYPE type;
    //
    //                ++packetCount;
    //
    //                if (packet->stream_index == d->m_videoStream) {
    //                    cbList = d->m_cbMap.keys(CB_VIDEO);
    //                    ++videoPackets;
    //                    type = CB_VIDEO;
    //                } else if (packet->stream_index == d->m_audioStream) {
    //                    cbList = d->m_cbMap.keys(CB_AUDIO);
    //                    qDebug("Audio packet duration: %f ms",
    //                           static_cast<double>(packet->duration) * 1000.0 * av_q2d(d->m_pFormatCtx->streams[d->m_audioStream]->time_base));
    //                    ++audioPackets;
    //                    type = CB_AUDIO;
    //                } else if (packet->stream_index == d->m_subtitleStream) {
    //                    cbList = d->m_cbMap.keys(CB_SUBTITLE);
    //                    ++sttPackets;
    //                    type = CB_SUBTITLE;
    //                } else {
    //                    av_packet_unref(packet);
    //                    continue;
    //                }
    //                for (const auto &cb : cbList) {
    //                    AVPacket *cbPacket = av_packet_clone(packet);
    //                    cb->onPacket(this, cbPacket, type);
    //                    av_packet_free(&cbPacket);
    //                }
    //                if (elapsedTimer.hasExpired(500)) {
    //                    QByteArray aP, vP, sP, aR, vR, sR;
    //                    aP = QString("%1").arg(audioPackets, 12).toLocal8Bit();
    //                    vP = QString("%1").arg(videoPackets, 12).toLocal8Bit();
    //                    sP = QString("%1").arg(sttPackets, 12).toLocal8Bit();
    //                    aR = QString("%1").arg((static_cast<double>(audioPackets) * 1.0 / static_cast<double>(packetCount)) * 100.0,
    //                                           10)
    //                                 .toLocal8Bit();
    //                    vR = QString("%1").arg((static_cast<double>(videoPackets) * 1.0 / static_cast<double>(packetCount)) * 100.0,
    //                                           10)
    //                                 .toLocal8Bit();
    //                    sR = QString("%1").arg((static_cast<double>(sttPackets) * 1.0 / static_cast<double>(packetCount)) * 100.0,
    //                                           10)
    //                                 .toLocal8Bit();
    //
    //                    qDebug() << "Packet statistics";
    //                    qDebug() << "| Packet type | Packet count |   Percentage |";
    //                    qDebug() << "| Video       |" << vP.data() << "|" << vR.data() << "% |";
    //                    qDebug() << "| Audio       |" << aP.data() << "|" << aR.data() << "% |";
    //                    qDebug() << "| Subtitle    |" << sP.data() << "|" << sR.data() << "% |";
    //                    qDebug();
    //                    elapsedTimer.restart();
    //                }
    //                av_packet_unref(packet);
    //            } else {
    //                msleep(8);
    //            }
    //        }
    //        av_packet_free(&packet);
    //    }
    //
    //    Demuxer::Demuxer(Demuxer &&other) noexcept : d_ptr(other.d_ptr) {
    //        other.d_ptr = nullptr;
    //        d_ptr->q_ptr = this;
    //    }
    //
    int DemuxerPrivate::readFromIO(void *opaque, uint8_t *buf, int bufSize) {
        auto *inputDevice = reinterpret_cast<QIODevice *>(opaque);

        auto bytesRead = inputDevice->read(reinterpret_cast<char *>(buf), bufSize);
        if (bytesRead == 0) {
            return AVERROR_EOF;
        } else {
            return static_cast<int>(bytesRead);
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

    bool DemuxerPrivate::linkValidator(const ProcessingGraph::Pad<AVPacket *> &pad1, const ProcessingGraph::Pad<AVPacket *> &pad2) {
        if (pad1.getOpaque().canConvert<PacketPadParams>() && pad2.getOpaque().canConvert<PacketPadParams>()) {
            return pad1.getOpaque().value<PacketPadParams>().mediaType == pad2.getOpaque().value<PacketPadParams>().mediaType;
        }
        return false;
    }
}// namespace AVQt
