// Copyright (c) 2021-2022.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "AVQt/input/Demuxer.hpp"
#include "private/Demuxer_p.hpp"

#include "AVQt/communication/Message.hpp"
#include "AVQt/communication/PacketPadParams.hpp"

#include "global.hpp"

#include <pgraph_network/api/PadRegistry.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace AVQt {
    [[maybe_unused]] Demuxer::Demuxer(Config config,
                                      std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry,
                                      QObject *parent)
        : QThread(parent),
          d_ptr(new DemuxerPrivate(this)),
          pgraph::impl::SimpleProducer(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)) {
        Q_D(AVQt::Demuxer);
        d->inputDevice = std::move(config.inputDevice);
        d->loop = config.loop;
    }

    bool Demuxer::isPaused() const {
        Q_D(const AVQt::Demuxer);
        return d->running.load() && d->paused.load();
    }

    bool Demuxer::isOpen() const {
        Q_D(const AVQt::Demuxer);
        return d->open.load();
    }

    bool Demuxer::isRunning() const {
        Q_D(const AVQt::Demuxer);
        return d->running.load();
    }

    bool Demuxer::init() {
        Q_D(AVQt::Demuxer);

        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            if (!d->inputDevice->isOpen()) {
                if (!d->inputDevice->open(QIODevice::ReadOnly)) {
                    qWarning() << "Could not open input device";
                    d->open.store(false);
                    return false;
                }
            }
            d->pBuffer = static_cast<uint8_t *>(av_malloc(DemuxerPrivate::BUFFER_SIZE));
            d->pIOCtx = {avio_alloc_context(d->pBuffer,
                                            DemuxerPrivate::BUFFER_SIZE,
                                            0,
                                            d,
                                            &DemuxerPrivate::readFromIO,
                                            nullptr,
                                            d->inputDevice->isSequential() ? nullptr : &DemuxerPrivate::seekIO),
                         &DemuxerPrivate::destroyAVIOContext};
            d->pFormatCtx = {avformat_alloc_context(), &DemuxerPrivate::destroyAVFormatContext};
            d->pFormatCtx->pb = d->pIOCtx.get();
            d->pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

            auto formatContext = d->pFormatCtx.get();
            if (avformat_open_input(&formatContext, "", nullptr, nullptr) < 0) {
                qFatal("Could not open input format context");
            }

            avformat_find_stream_info(d->pFormatCtx.get(), nullptr);

            for (int64_t si = 0; si < d->pFormatCtx->nb_streams; ++si) {
                switch (d->pFormatCtx->streams[si]->codecpar->codec_type) {
                    case AVMEDIA_TYPE_VIDEO:
                        d->videoStreams.append(si);
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                        d->audioStreams.append(si);
                        break;
                    case AVMEDIA_TYPE_SUBTITLE:
                        d->subtitleStreams.append(si);
                        break;
                    default:
                        break;
                }
                auto packetPadParams = std::make_shared<communication::PacketPadParams>();
                packetPadParams->mediaType = d->pFormatCtx->streams[si]->codecpar->codec_type;
                packetPadParams->codec = d->pFormatCtx->streams[si]->codecpar->codec_id;
                packetPadParams->streamIdx = si;
                packetPadParams->codecParams = std::shared_ptr<AVCodecParameters>(avcodec_parameters_alloc(), [](AVCodecParameters *p) {
                    avcodec_parameters_free(&p);
                });
                avcodec_parameters_copy(packetPadParams->codecParams.get(), d->pFormatCtx->streams[si]->codecpar);
                auto padId = createOutputPad(packetPadParams);
                if (padId == pgraph::api::INVALID_PAD_ID) {
                    qWarning() << "Failed to create output pad";
                    return false;
                }
                d->outputPadIds.insert(si, padId);
                qDebug("Creating pad %ld for stream %ld", d->outputPadIds[si], si);
            }
        } else {
            qWarning() << "Demuxer already initialized";
            return false;
        }
        return true;
    }

    bool Demuxer::open() {
        Q_D(AVQt::Demuxer);

        if (!d->initialized) {
            qWarning() << "Demuxer not initialized";
            return false;
        }
        if (d->running) {
            qWarning() << "Demuxer already running";
            return false;
        }
        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            const auto streams = d->outputPadIds.keys();
            for (const auto &stream : streams) {
                auto packetParams = std::dynamic_pointer_cast<const communication::PacketPadParams>(getOutputPad(d->outputPadIds.value(stream))->getUserData());
                produce(communication::Message::builder()
                                .withAction(communication::Message::Action::INIT)
                                .withPayload("packetParams", QVariant::fromValue(packetParams))
                                .build(),
                        d->outputPadIds[stream]);
            }
            return true;
        } else {
            qWarning() << "Demuxer already open";
            return false;
        }
    }

    void Demuxer::close() {
        Q_D(AVQt::Demuxer);

        Demuxer::stop();

        for (const auto &pad : d->outputPadIds) {
            produce(communication::Message::builder().withAction(communication::Message::Action::CLEANUP).build(), pad);
        }

        if (d->open) {
            d->open.store(false);
            d->running.store(false);
            d->initialized.store(false);
            d->inputDevice->close();
            d->pFormatCtx.reset();
            d->pIOCtx.reset();
        } else {
            qWarning() << "Demuxer not open";
        }

        for (const auto &pad : d->outputPadIds) {
            destroyOutputPad(pad);
        }
    }

    bool Demuxer::start() {
        Q_D(AVQt::Demuxer);

        if (!d->initialized) {
            qWarning() << "Demuxer not initialized";
            return false;
        }

        if (!d->open) {
            qWarning() << "Demuxer not open";
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->paused = false;
            for (const auto &pad : d->outputPadIds) {
                produce(communication::Message::builder().withAction(communication::Message::Action::START).build(), pad);
            }

            QThread::start();

            return true;
        }
        return false;
    }

    void Demuxer::stop() {
        Q_D(AVQt::Demuxer);

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = false;
            QThread::quit();
            QThread::wait();
            for (const auto &pad : d->outputPadIds) {
                produce(communication::Message::builder().withAction(communication::Message::Action::STOP).build(), pad);
            }
            emit stopped();
        }
    }

    void Demuxer::pause(bool pause) {
        Q_D(AVQt::Demuxer);

        bool pauseFlag = !pause;
        if (d->paused.compare_exchange_strong(pauseFlag, pause)) {
            d->paused.store(pause);
            for (const auto &pad : d->outputPadIds) {
                produce(communication::Message::builder().withAction(communication::Message::Action::PAUSE).withPayload("state", pause).build(), pad);
            }
            emit paused(pause);
        }
    }

    void Demuxer::run() {
        Q_D(AVQt::Demuxer);

        emit started();

        int ret;
        constexpr size_t strBufSize = 1024;
        char strBuf[strBufSize];

        std::shared_ptr<AVPacket> packet{};

        while (d->running) {
            if (d->paused) {
                msleep(2);
                continue;
            }

            packet = {av_packet_alloc(), [](AVPacket *p) {
                          av_packet_free(&p);
                      }};

            static int i = 0;
            ret = av_read_frame(d->pFormatCtx.get(), packet.get());
            ++i;

            if (ret == AVERROR(EAGAIN)) {
                continue;
            } else if (ret == AVERROR_EOF) {
                if (d->loop) {
                    ret = avformat_seek_file(d->pFormatCtx.get(), -1, INT64_MIN, 0, INT64_MAX, 0);
                    if (ret < 0) {
                        qWarning() << Q_FUNC_INFO << "Error while seeking";
                        break;
                    }
                    continue;
                } else {
                    break;
                }
            } else if (ret < 0) {
                qDebug() << Q_FUNC_INFO << "Error reading frame:" << av_make_error_string(strBuf, strBufSize, ret);
                break;
            }

            if (d->outputPadIds.contains(packet->stream_index)) {
                av_packet_rescale_ts(packet.get(), d->pFormatCtx->streams[packet->stream_index]->time_base, {1, 1000000});
                auto message = communication::Message::builder()
                                       .withAction(communication::Message::Action::DATA)
                                       .withPayload("packet", QVariant::fromValue(packet))
                                       .build();
                produce(message, d->outputPadIds[packet->stream_index]);
            }
        }
    }

    int DemuxerPrivate::readFromIO(void *opaque, uint8_t *buf, int bufSize) {
        auto *d = reinterpret_cast<DemuxerPrivate *>(opaque);

        auto bytesRead = d->inputDevice->read(reinterpret_cast<char *>(buf), bufSize);
        if (bytesRead == 0) {
            return AVERROR_EOF;
        } else {
            return static_cast<int>(bytesRead);
        }
    }

    int64_t DemuxerPrivate::seekIO(void *opaque, int64_t pos, int whence) {
        auto *d = reinterpret_cast<DemuxerPrivate *>(opaque);

        if (d->inputDevice->isSequential()) {
            return -1;
        }

        bool result;
        switch (whence) {
            case SEEK_SET:
                result = d->inputDevice->seek(pos);
                break;
            case SEEK_CUR:
                result = d->inputDevice->seek(d->inputDevice->pos() + pos);
                break;
            case SEEK_END:
                result = d->inputDevice->seek(d->inputDevice->size() - pos);
                break;
            case AVSEEK_SIZE:
                return d->inputDevice->size();
            default:
                return -1;
        }

        if (result) {
            return d->inputDevice->pos();
        } else {
            return -1;
        }
    }

    void DemuxerPrivate::destroyAVIOContext(AVIOContext *context) {
        if (context) {
            av_freep(&context->buffer);
            av_freep(&context);
        }
    }

    void DemuxerPrivate::destroyAVFormatContext(AVFormatContext *context) {
        if (context) {
            avformat_close_input(&context);
            avformat_free_context(context);
            context = nullptr;
        }
    }
}// namespace AVQt
