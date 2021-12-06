#include "Demuxer.h"
#include "output/IPacketSink.h"
#include "private/Demuxer_p.h"

#include <communication/PacketPadParams.h>
#include <pgraph/api/PadUserData.hpp>
#include <pgraph/impl/SimplePadFactory.hpp>
#include <pgraph_network/api/PadRegistry.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

Q_DECLARE_METATYPE(AVPacket *)

namespace AVQt {
    [[maybe_unused]] Demuxer::Demuxer(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent), d_ptr(new DemuxerPrivate(this)),
          pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)) {
        Q_D(AVQt::Demuxer);
    }

    Demuxer::Demuxer(DemuxerPrivate &p)
        : d_ptr(&p),
          pgraph::impl::SimpleProcessor(pgraph::impl::SimplePadFactory::getInstance()) {
    }

    void Demuxer::pause(bool pause) {
        Q_D(AVQt::Demuxer);

        bool pauseFlag = !pause;
        if (d->m_paused.compare_exchange_strong(pauseFlag, pause)) {
            d->m_paused.store(pause);
            produce(Message::builder().withAction(Message::Action::PAUSE).withPayload("state", pause).build(), d->m_commandOutputPadId);
            paused(pause);
        }
    }

    bool Demuxer::isPaused() {
        Q_D(AVQt::Demuxer);
        return d->m_paused.load();
    }

    bool Demuxer::open() {
        Q_D(AVQt::Demuxer);
        bool shouldBe = false;
        if (d->m_initialized.compare_exchange_strong(shouldBe, true)) {
            d->m_pBuffer = static_cast<uint8_t *>(av_malloc(DemuxerPrivate::BUFFER_SIZE));
            d->m_pIOCtx = avio_alloc_context(d->m_pBuffer, DemuxerPrivate::BUFFER_SIZE, 0, d,
                                             &DemuxerPrivate::readFromIO, nullptr, nullptr);
            d->m_pFormatCtx = avformat_alloc_context();
            d->m_pFormatCtx->pb = d->m_pIOCtx;
            d->m_pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;
        }

        return true;
    }

    void Demuxer::close() {
        Q_D(AVQt::Demuxer);

//        d->deinitPads();

        const auto pads{d->m_messagePadIds.values()};

        for (const auto &pad : pads) {
            pgraph::impl::SimpleProcessor::destroyOutputPad(pad);
        }

        d->m_messagePadIds.clear();
        pgraph::impl::SimpleProcessor::destroyOutputPad(d->m_commandOutputPadId);
    }

    uint32_t Demuxer::getCommandOutputPadId() const {
        Q_D(const AVQt::Demuxer);
        return d->m_commandOutputPadId;
    }

    uint32_t Demuxer::getInputPadId() const {
        Q_D(const AVQt::Demuxer);
        return d->m_inputPadId;
    }

    void Demuxer::consume(uint32_t padId, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(AVQt::Demuxer);
        if (d->m_inputPadId == padId && data->getType() == Message::Type) {
            std::shared_ptr<Message> message = std::dynamic_pointer_cast<Message>(data);
            //            qDebug() << Q_FUNC_INFO << message->getAction().name() << message->getPayloads();
            switch ((Message::Action::Enum) message->getAction()) {
                case Message::Action::INIT:
                    open();
                    break;
                case Message::Action::CLEANUP:
                    close();
                    break;
                case Message::Action::START:
                    start();
                    break;
                case Message::Action::STOP:
                    stop();
                    break;
                case Message::Action::PAUSE:
                    pause(message->getPayload("state").toBool());
                    break;
                case Message::Action::DATA:
                    d->enqueueData(message->getPayload("data").toByteArray());
                    break;
                default:
                    qWarning() << Q_FUNC_INFO << "Unknown action";
                    break;
            }
        }
    }

    bool Demuxer::start() {
        Q_D(AVQt::Demuxer);

        bool shouldBe = false;
        if (d->m_running.compare_exchange_strong(shouldBe, true)) {
            qDebug() << "Input queue: " << d->m_inputData.size() * DemuxerPrivate::INPUT_BLOCK_SIZE << "B";
            if (avformat_open_input(&d->m_pFormatCtx, "", nullptr, nullptr) < 0) {
                qFatal("Could not open input format context");
            }

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
                auto packetPadParams = std::make_shared<PacketPadParams>();
                packetPadParams->mediaType = d->m_pFormatCtx->streams[si]->codecpar->codec_type;
                packetPadParams->codec = d->m_pFormatCtx->streams[si]->codecpar->codec_id;
                packetPadParams->streamIdx = si;
                d->m_messagePadIds.insert(si, pgraph::impl::SimpleProcessor::createOutputPad(packetPadParams));
                qDebug("Creating pad %ul", d->m_messagePadIds[si]);
            }

            pgraph::impl::SimpleProcessor::produce(Message::builder().withAction(Message::Action::INIT).build(), d->m_commandOutputPadId);

            QThread::start();

            return true;
        }
        return false;
    }

    void Demuxer::stop() {
        Q_D(AVQt::Demuxer);

        bool shouldBe = true;
        if (d->m_running.compare_exchange_strong(shouldBe, false)) {
            d->m_inputDataCond.wakeAll();
            QThread::quit();
            QThread::wait();
            pgraph::impl::SimpleProcessor::produce(Message::builder().withAction(Message::Action::CLEANUP).build(), d->m_commandOutputPadId);
        }
    }
    void Demuxer::init() {
        Q_D(AVQt::Demuxer);

        d->m_commandOutputPadId = pgraph::impl::SimpleProcessor::createOutputPad(pgraph::api::PadUserData::emptyUserData());
        d->m_inputPadId = pgraph::impl::SimpleProcessor::createInputPad(pgraph::api::PadUserData::emptyUserData());
    }
    void Demuxer::run() {
        Q_D(AVQt::Demuxer);

        auto streamids = d->m_messagePadIds.keys();
        for (const int64_t &si : streamids) {
            AVCodecParameters *params = avcodec_parameters_alloc();
            avcodec_parameters_copy(params, d->m_pFormatCtx->streams[si]->codecpar);
            pgraph::impl::SimpleProcessor::produce(Message::builder()
                                                           .withAction(Message::Action::INIT)
                                                           .withPayload("streamIdx", QVariant::fromValue(si))
                                                           .withPayload("codecparams", QVariant::fromValue(params))
                                                           .build(),
                                                   d->m_messagePadIds[si]);
            avcodec_parameters_free(&params);
        }

        pgraph::impl::SimpleProcessor::produce(Message::builder().withAction(Message::Action::START).build(), d->m_commandOutputPadId);

        int ret;
        constexpr size_t strBufSize = 1024;
        char strBuf[strBufSize];

        AVPacket *packet = av_packet_alloc();

        while (d->m_running) {
            if (d->m_paused || d->m_inputData.isEmpty()) {
                QThread::msleep(2);
                continue;
            }
            //            if (d->m_pFormatCtx->pb->eof_reached) {
            //                qDebug() << Q_FUNC_INFO << "End of file reached";
            //                break;
            //            }
            if (d->m_pFormatCtx->pb->error) {
                qDebug() << Q_FUNC_INFO << "Error occured";
                break;
            }
            {
                ret = av_read_frame(d->m_pFormatCtx, packet);

                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN) || ret == AVERROR_EXIT) {
                    av_packet_unref(packet);
                    break;
                } else if (ret < 0) {
                    qDebug() << Q_FUNC_INFO << "Error reading frame:" << av_make_error_string(strBuf, strBufSize, ret);
                    break;
                }

                if (d->m_messagePadIds.contains(packet->stream_index)) {
                    av_packet_rescale_ts(packet, d->m_pFormatCtx->streams[packet->stream_index]->time_base, {1, 1000000});
                    auto message = Message::builder().withAction(Message::Action::DATA).withPayload("packet", QVariant::fromValue(packet)).build();
                    pgraph::impl::SimpleProcessor::produce(message, d->m_messagePadIds[packet->stream_index]);
                }
            }

            av_packet_unref(packet);
        }
        pgraph::impl::SimpleProcessor::produce(Message::builder().withAction(Message::Action::STOP).build(), d->m_commandOutputPadId);
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
    //            packetSink->open(this, d->m_pFormatCtx->streams[d->m_videoStream]->avg_frame_rate, av_make_q(1, AV_TIME_BASE),
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
    //                packetSink->close(this);
    //            }
    //
    //            return 0;
    //        } else {
    //            return -1;
    //        }
    //    }
    //
    //
    //    int Demuxer::close() {
    //        Q_D(AVQt::Demuxer);
    //
    //        stop();
    //
    //        auto cbs = d->m_cbMap.keys();
    //        for (const auto &cb : cbs) {
    //            cb->close(this);
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
        auto *d = reinterpret_cast<DemuxerPrivate *>(opaque);
        if (!d->m_running.load()) {
            return AVERROR_EOF;
        }
        QMutexLocker lock(&d->m_inputDataMutex);
        int bytesRead = 0;
        if (d->m_inputData.isEmpty()) {
            d->m_inputDataMutex.unlock();
            d->m_inputDataCond.wait(&d->m_inputDataMutex);
            if (!d->m_running.load()) {
                return AVERROR_EXIT;
            }
        }
        while (!d->m_inputData.isEmpty() && bytesRead < bufSize) {
            auto &data = d->m_inputData.first();
            int toRead = qMin(bufSize - bytesRead, data.size());
            memcpy(buf + bytesRead, data.constData(), toRead);
            bytesRead += toRead;
            data.remove(0, toRead);
            if (data.isEmpty()) {
                d->m_inputData.removeFirst();
            }
        }

        return bytesRead == 0 ? AVERROR_EOF : bytesRead;
        //        auto *inputDevice = reinterpret_cast<QIODevice *>(opaque);
        //
        //        auto bytesRead = inputDevice->read(reinterpret_cast<char *>(buf), bufSize);
        //        if (bytesRead == 0) {
        //            return AVERROR_EOF;
        //        } else {
        //            return static_cast<int>(bytesRead);
        //        }
    }

    int64_t DemuxerPrivate::seekIO(void *opaque, int64_t pos, int whence) {
        return -1;
        //        auto *inputDevice = reinterpret_cast<QIODevice *>(opaque);
        //
        //        if (inputDevice->isSequential()) {
        //            return -1;
        //        }
        //
        //        bool result;
        //        switch (whence) {
        //            case SEEK_SET:
        //                result = inputDevice->seek(pos);
        //                break;
        //            case SEEK_CUR:
        //                result = inputDevice->seek(inputDevice->pos() + pos);
        //                break;
        //            case SEEK_END:
        //                result = inputDevice->seek(inputDevice->size() - pos);
        //                break;
        //            case AVSEEK_SIZE:
        //                return inputDevice->size();
        //            default:
        //                return -1;
        //        }
        //
        //        if (result) {
        //            return inputDevice->pos();
        //        } else {
        //            return -1;
        //        }
    }

    void DemuxerPrivate::initPads() {
        Q_Q(AVQt::Demuxer);

        for (int64_t si = 0; si < m_pFormatCtx->nb_streams; ++si) {
            AVCodecParameters *parameters = avcodec_parameters_alloc();
            avcodec_parameters_copy(parameters, m_pFormatCtx->streams[si]->codecpar);
            q->produce(Message::builder().withAction(Message::Action::INIT).withPayload("codec_par", QVariant::fromValue(parameters)).build(), m_messagePadIds[si]);
        }
    }

    void DemuxerPrivate::deinitPads() {
        Q_Q(AVQt::Demuxer);
        q->produce(Message::builder().withAction(Message::Action::CLEANUP).build(), m_commandOutputPadId);
    }

    void DemuxerPrivate::enqueueData(QByteArray data) {
        static int64_t callCount = 0;
        //        if (callCount++ % 100 == 0) {
        //            qDebug() << "DemuxerPrivate::enqueueData" << data.size();
        //        }
        if (!data.isEmpty()) {
            //            qDebug("Enqueuing %d B of data", data.size());

            QMutexLocker lock(&m_inputDataMutex);
            if (m_inputData.isEmpty()) {
                //            qDebug("%ld: Enqueueing %d B of data", callCount++, data.size());
                m_inputData.enqueue(data);
            } else {
                while (data.size() > 0) {
                    if (m_inputData.last().size() < INPUT_BLOCK_SIZE) {
                        auto last = m_inputData.last();
                        auto lastSize = last.size();
                        auto toAppend = qMin(data.size(), INPUT_BLOCK_SIZE - lastSize);
                        //                        qDebug("Appending %d B to last element", qMin(data.size(), INPUT_BLOCK_SIZE - m_inputData.last().size()));
                        //                        qDebug() << "Concatenating:" << last.toHex() << "+" << data.toHex();
                        m_inputData.last().append(data.left(toAppend));
                        //                        qDebug() << "Result:" << m_inputData.last().toHex() << "+" << data.toHex();
                        if (!m_inputData.last().contains(last) || !m_inputData.last().contains(data.left(toAppend)) || m_inputData.last().size() != toAppend + lastSize) {
                            qFatal("Concatenation failed");
                        }
                        data.remove(0, toAppend);
                    } else {
                        //                        qDebug("Enqueuing new element with %d B", qMin(data.size(), INPUT_BLOCK_SIZE));
                        m_inputData.enqueue(data.left(INPUT_BLOCK_SIZE));
                        data.remove(0, INPUT_BLOCK_SIZE);
                    }
                }
            }
            m_inputDataCond.wakeAll();
        } else {
            qWarning() << Q_FUNC_INFO << "Empty data received";
        }
    }
}// namespace AVQt

Q_DECLARE_METATYPE(AVCodecParameters *)
