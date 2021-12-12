#include "input/IPacketSource.hpp"
#include "private/Muxer_p.hpp"
#include "output/Muxer.hpp"

namespace AVQt {
    Muxer::Muxer(QIODevice *outputDevice, FORMAT format, QObject *parent) : QThread(parent), d_ptr(new MuxerPrivate(this)) {
        Q_D(AVQt::Muxer);
        d->m_outputDevice = outputDevice;
        d->m_format = format;
    }

    Muxer::Muxer(AVQt::MuxerPrivate &p) : d_ptr(&p) {

    }

    AVQt::Muxer::Muxer(Muxer &&other) noexcept: d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
    }

    bool Muxer::isPaused() {
        Q_D(AVQt::Muxer);
        return d->m_paused.load();
    }

    void Muxer::init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                     AVCodecParameters *aParams, AVCodecParameters *sParams) {
        Q_UNUSED(framerate)
        Q_UNUSED(duration)
        Q_D(AVQt::Muxer);

        if ((vParams && aParams) || (vParams && sParams) || (aParams && sParams)) {
            qWarning("[AVQt::Muxer] open() called for multiple stream types at once. Ignoring call");
            return;
        }

        if (d->m_sourceStreamMap.contains(source)) {
            bool alreadyCalled = false;
            for (const auto &stream: d->m_sourceStreamMap[source].keys()) {
                if ((vParams && stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) ||
                    (aParams && stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) ||
                    (sParams && stream->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)) {
                    alreadyCalled = true;
                    break;
                }
            }
            if (alreadyCalled) {
                qWarning("[AVQt::Muxer] open() called multiple times for the same stream type by the same source. Ignoring call");
                return;
            }
        }
        QMutexLocker lock(&d->m_initMutex);
        if (!d->m_pFormatContext) {
            if (!d->m_outputDevice->isOpen()) {
                if (!d->m_outputDevice->open((d->m_outputDevice->isSequential() ? QIODevice::WriteOnly : QIODevice::ReadWrite))) {
                    qFatal("[AVQt::Muxer] Could not open output device");
                }
            } else if (!d->m_outputDevice->isWritable()) {
                qFatal("[AVQt::Muxer] Output device is not writable");
            }

            QString outputFormat;
            switch (d->m_format) {
                case FORMAT::MP4:
                    if (d->m_outputDevice->isSequential()) {
                        qFatal("[AVQt::Muxer] MP4 output format is not available on sequential output devices like sockets");
                    }
                    outputFormat = "mp4";
                    break;
                case FORMAT::MOV:
                    if (d->m_outputDevice->isSequential()) {
                        qFatal("[AVQt::Muxer] MOV output format is not available on sequential output devices like sockets");
                    }
                    outputFormat = "mov";
                    break;
                case FORMAT::MKV:
                    outputFormat = "matroska";
                    break;
                case FORMAT::WEBM:
                    outputFormat = "webm";
                    break;
                case FORMAT::MPEGTS:
                    outputFormat = "mpegts";
                    break;
                case FORMAT::INVALID:
                    qFatal("[AVQt::Muxer] FORMAT::INVALID is just a placeholder, don't pass it as an argument");
            }

            d->m_pFormatContext = avformat_alloc_context();
            d->m_pFormatContext->oformat = av_guess_format(outputFormat.toLocal8Bit().data(), "", nullptr);
            d->m_pIOBuffer = static_cast<uint8_t *>(av_malloc(MuxerPrivate::IOBUF_SIZE));
            d->m_pIOContext = avio_alloc_context(d->m_pIOBuffer, MuxerPrivate::IOBUF_SIZE, 1, d, nullptr,
                                                 &MuxerPrivate::writeToIO, &MuxerPrivate::seekIO);
            d->m_pIOContext->seekable = !d->m_outputDevice->isSequential();
            d->m_pFormatContext->pb = d->m_pIOContext;
            d->m_pFormatContext->flags |= AVFMT_FLAG_CUSTOM_IO;
        }

        if (!d->m_sourceStreamMap.contains(source)) {
            d->m_sourceStreamMap[source] = QMap<AVStream *, AVRational>();
        }
        if (vParams) {
            AVStream *videoStream = avformat_new_stream(d->m_pFormatContext, avcodec_find_encoder(vParams->codec_id));
            videoStream->codecpar = avcodec_parameters_alloc();
            avcodec_parameters_copy(videoStream->codecpar, vParams);
            videoStream->time_base = timebase;
            d->m_sourceStreamMap[source].insert(videoStream, timebase);
        } else if (aParams) {
            AVStream *audioStream = avformat_new_stream(d->m_pFormatContext, avcodec_find_encoder(aParams->codec_id));
            audioStream->codecpar = avcodec_parameters_alloc();
            avcodec_parameters_copy(audioStream->codecpar, aParams);
            audioStream->time_base = timebase;
            d->m_sourceStreamMap[source].insert(audioStream, timebase);
        } else if (sParams) {
            AVStream *subtitleStream = avformat_new_stream(d->m_pFormatContext, avcodec_find_encoder(sParams->codec_id));
            subtitleStream->codecpar = avcodec_parameters_alloc();
            avcodec_parameters_copy(subtitleStream->codecpar, sParams);
            subtitleStream->time_base = timebase;
            d->m_sourceStreamMap[source].insert(subtitleStream, timebase);
        }
        qDebug("[AVQt::Muxer] Initialized");
    }

    void Muxer::deinit(IPacketSource *source) {
        Q_D(AVQt::Muxer);

        stop(source);
        qDebug("[AVQt::Muxer] close() called");

        if (d->m_sourceStreamMap.contains(source)) {
            d->m_sourceStreamMap.remove(source);
        } else {
            qWarning("[AVQt::Muxer] close() called without preceding open() from source. Ignoring call");
            return;
        }

        if (d->m_sourceStreamMap.isEmpty()) {
            QMutexLocker lock(&d->m_initMutex);
            if (d->m_pFormatContext) {
                int ret = av_write_frame(d->m_pFormatContext, nullptr);
                if (ret != 0) {
                    constexpr auto strBufSize = 32;
                    char strBuf[strBufSize];
                    qWarning("%d: Couldn't flush AVFormatContext packet queue: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }
//                if (d->m_headerWritten.load()) {
                av_write_trailer(d->m_pFormatContext);
                qDebug("[AVQt::Muxer] Wrote trailer");
//                }
                avio_flush(d->m_pFormatContext->pb);
                avformat_free_context(d->m_pFormatContext);
                d->m_outputDevice->close();
            }
        }

    }

    void Muxer::start(IPacketSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::Muxer);

        bool shouldBe = false;
        if (d->m_running.compare_exchange_strong(shouldBe, true)) {
            shouldBe = false;
//            if (d->m_headerWritten.compare_exchange_strong(shouldBe, true)) {
//
//            }
            d->m_paused.store(false);
            QThread::start();
            started();
        }
    }

    void Muxer::stop(IPacketSource *source) {
        Q_UNUSED(source)
        Q_D(AVQt::Muxer);

        bool shouldBe = true;
        if (d->m_running.compare_exchange_strong(shouldBe, false)) {
            d->m_paused.store(false);
            wait();
            {
                QMutexLocker lock{&d->m_inputQueueMutex};
                while (!d->m_inputQueue.isEmpty()) {
                    auto packet = d->m_inputQueue.dequeue();
                    av_packet_free(&packet.first);
                }
            }
            stopped();
        }
    }

    void Muxer::pause(bool p) {
        Q_D(AVQt::Muxer);

        bool shouldBe = !p;
        if (d->m_paused.compare_exchange_strong(shouldBe, p)) {
            paused(p);
        }
    }

    void Muxer::onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) {
        Q_D(AVQt::Muxer);

        if (packet->pts != AV_NOPTS_VALUE) {

            bool unknownSource = !d->m_sourceStreamMap.contains(source);
            bool initStream = false;
            AVStream *addStream;
            if (!unknownSource) {
                for (const auto &stream : d->m_sourceStreamMap[source].keys()) {
                    if ((packetType == IPacketSource::CB_VIDEO && stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) ||
                        (packetType == IPacketSource::CB_AUDIO && stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) ||
                        (packetType == IPacketSource::CB_SUBTITLE && stream->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)) {
                        addStream = stream;
                        initStream = true;
                        break;
                    }
                }
            }
            if (unknownSource || !initStream) {
                qWarning("[AVQt::Muxer] onPacket() called without preceding call to open() for stream type. Ignoring packet");
                return;
            }

            QPair<AVPacket *, AVStream *> queuePacket{av_packet_clone(packet), addStream};
            queuePacket.first->stream_index = addStream->index;
            qDebug("[AVQt::Muxer] Getting packet with PTS: %lld", static_cast<long long>(packet->pts));
            av_packet_rescale_ts(queuePacket.first, av_make_q(1, 1000000), addStream->time_base);

            while (d->m_inputQueue.size() >= 100) {
                QThread::msleep(2);
            }

            QMutexLocker lock(&d->m_inputQueueMutex);
            d->m_inputQueue.enqueue(queuePacket);
//            std::sort(d->m_inputQueue.begin(), d->m_inputQueue.end(), &MuxerPrivate::packetQueueCompare);
        }
    }

    void Muxer::run() {
        Q_D(AVQt::Muxer);

        int ret = avformat_write_header(d->m_pFormatContext, nullptr);
        if (ret != 0) {
            constexpr auto strBufSize = 32;
            char strBuf[strBufSize];
            qWarning("[AVQt::Muxer] %d: Couldn't open AVFormatContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
        }

        while (d->m_running.load()) {
            if (!d->m_paused.load() && d->m_inputQueue.size() > 5) {
                QPair<AVPacket *, AVStream *> packet;
                {
                    QMutexLocker lock(&d->m_inputQueueMutex);
                    packet = d->m_inputQueue.dequeue();
                }
                int ret = av_interleaved_write_frame(d->m_pFormatContext, packet.first);
                qDebug("Written packet");
                if (ret != 0) {
                    constexpr auto strBufSize = 32;
                    char strBuf[strBufSize];
                    qWarning("%d: Couldn't write packet to AVFormatContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }
            } else {
                msleep(4);
            }
        }
    }

    int MuxerPrivate::writeToIO(void *opaque, uint8_t *buf, int buf_size) {
        auto *d = reinterpret_cast<MuxerPrivate *>(opaque);
        QMutexLocker lock(&d->m_ioMutex);

        auto bytesWritten = d->m_outputDevice->write(reinterpret_cast<const char *>(buf), buf_size);
        return bytesWritten == 0 ? AVERROR_UNKNOWN : static_cast<int>(bytesWritten);
    }

    int64_t MuxerPrivate::seekIO(void *opaque, int64_t offset, int whence) {
        auto *d = reinterpret_cast<MuxerPrivate *>(opaque);
        QMutexLocker lock(&d->m_ioMutex);

        if (d->m_outputDevice->isSequential()) {
            return AVERROR_UNKNOWN;
        }

        bool result;
        switch (whence) {
            case SEEK_SET:
                result = d->m_outputDevice->seek(offset);
                break;
            case SEEK_CUR:
                result = d->m_outputDevice->seek(d->m_outputDevice->pos() + offset);
                break;
            case SEEK_END:
                result = d->m_outputDevice->seek(d->m_outputDevice->size() - offset);
                break;
            case AVSEEK_SIZE:
                return d->m_outputDevice->size();
            default:
                return AVERROR_UNKNOWN;
        }

        return result ? d->m_outputDevice->pos() : AVERROR_UNKNOWN;
    }

    bool MuxerPrivate::packetQueueCompare(const QPair<AVPacket *, AVStream *> &packet1, const QPair<AVPacket *, AVStream *> &packet2) {
        return packet1.first->dts < packet2.first->dts;
    }
}
