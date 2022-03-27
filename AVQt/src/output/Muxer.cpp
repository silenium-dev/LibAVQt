//
// Created by silas on 27/03/2022.
//
#include "output/Muxer.hpp"
#include "private/Muxer_p.hpp"

#include "communication/Message.hpp"
#include "communication/PacketPadParams.hpp"

#include "global.hpp"

#include <pgraph/api/PadUserData.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace AVQt {
    Muxer::Muxer(Config config, QObject *parent) : QThread(parent), d_ptr(new MuxerPrivate(this)) {
        Q_D(Muxer);
        d->init(std::move(config));
    }

    Muxer::Muxer(Config config, MuxerPrivate *p, QObject *parent) : QThread(parent), d_ptr(p) {
        Q_D(Muxer);
        d->init(std::move(config));
    }

    void MuxerPrivate::init(AVQt::Muxer::Config config) {
        Q_Q(Muxer);
        outputDevice = std::move(config.outputDevice);
        pOutputFormat = av_guess_format(config.containerFormat, nullptr, nullptr);
        if (!pOutputFormat) {
            qWarning() << "[Muxer] Could not find output format for " << config.containerFormat;
            return;
        }

        if (!outputDevice->isOpen()) {
            if (!outputDevice->open(QIODevice::WriteOnly)) {
                qWarning() << "[Muxer] Could not open output device";
                return;
            }
        } else if (!outputDevice->isWritable()) {
            qWarning() << "[Muxer] Output device is not writable";
            return;
        }

        pFormatCtx.reset(avformat_alloc_context());
        pBuffer = static_cast<uint8_t *>(av_malloc(MuxerPrivate::BUFFER_SIZE));
        pIOCtx.reset(avio_alloc_context(pBuffer, MuxerPrivate::BUFFER_SIZE, AVIO_FLAG_WRITE, this, nullptr, MuxerPrivate::writeIO, MuxerPrivate::seekIO));
        pFormatCtx->pb = pIOCtx.get();
        pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;
        pFormatCtx->oformat = const_cast<AVOutputFormat *>(pOutputFormat);

        int ret = avformat_init_output(pFormatCtx.get(), nullptr);
        if (ret < 0) {
            char strBuf[AV_ERROR_MAX_STRING_SIZE];
            qWarning() << "[Muxer] Could not init output:" << av_make_error_string(strBuf, AV_ERROR_MAX_STRING_SIZE, ret);
            goto fail;
        }

        return;
    fail:
        qWarning() << "[Muxer] Could not initialize muxer";
        av_free(pIOCtx->buffer);
        pIOCtx.reset();
        pFormatCtx.reset();
        pBuffer = nullptr;
    }

    Muxer::~Muxer() {
        Q_D(Muxer);

        if (d->running) {
            Muxer::stop();
        }

        if (d->pBuffer) {
            av_free(d->pBuffer);
        }

        d->pFormatCtx.reset();
        d->pIOCtx.reset();
        d->outputDevice->close();
    }

    bool Muxer::init() {
        // Empty
        return true;
    }

    bool Muxer::open() {
        // Empty
        return true;
    }

    void Muxer::close() {
        // Empty
    }

    bool Muxer::start() {
        Q_D(Muxer);

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            int ret = avformat_write_header(d->pFormatCtx.get(), nullptr);
            if (ret < 0) {
                char strBuf[AV_ERROR_MAX_STRING_SIZE];
                qWarning() << "[Muxer] Could not write header:" << av_make_error_string(strBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                return false;
            }
            d->paused = false;
            QThread::start();
            return true;
        } else {
            qWarning() << "[Muxer] Already running";
            return false;
        }
    }

    void Muxer::stop() {
        Q_D(Muxer);

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = false;
            d->inputQueueCondition.notify_all();
            QThread::quit();
            QThread::wait();
            d->inputQueue.clear();
            emit stopped();
        } else {
            qWarning() << "[Muxer] Not running";
        }
    }

    void Muxer::pause(bool state) {
        Q_D(Muxer);

        bool shouldBe = !state;
        if (d->paused.compare_exchange_strong(shouldBe, state)) {
            d->pausedCond.notify_all();
            emit paused(state);
        } else {
            qDebug() << "Muxer::pause() called while already in state" << (state ? "paused" : "running");
        }
    }

    void Muxer::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(Muxer);

        if (data->getType() == communication::Message::Type) {
            auto msg = std::dynamic_pointer_cast<communication::Message>(data);
            switch (msg->getAction()) {
                case communication::Message::Action::INIT: {
                    if (!d->initStream(pad, msg->getPayload("packetParams").value<std::shared_ptr<communication::PacketPadParams>>())) {
                        qWarning() << "[Muxer] failed to open stream" << pad;
                    }
                    break;
                }
                case communication::Message::Action::START: {
                    if (!d->startStream(pad)) {
                        qWarning() << "[Muxer] failed to start stream" << pad;
                    }
                    break;
                }
                case communication::Message::Action::STOP: {
                    d->stopStream(pad);
                    break;
                }
                case communication::Message::Action::PAUSE: {
                    auto newState = msg->getPayload("state").toBool();
                    pause(newState);
                    break;
                }
                case communication::Message::Action::DATA: {
                    auto packet = msg->getPayload("packet").value<std::shared_ptr<AVPacket>>();
                    packet->stream_index = d->streams[pad]->index;
                    d->enqueueData(packet);
                    break;
                }
                // TODO: reset
                default:// CLEANUP will be handled by the destructor
                    break;
            }
        }
    }

    [[maybe_unused]] int64_t Muxer::createStreamPad(const std::shared_ptr<communication::PacketPadParams> &padData) {
        Q_D(Muxer);

        if (d->running) {
            qWarning() << "[Muxer] cannot create stream pad while running";
            return pgraph::api::INVALID_PAD_ID;
        }

        auto padId = pgraph::impl::SimpleConsumer::createInputPad(padData);
        if (padId == pgraph::api::INVALID_PAD_ID) {
            qWarning() << "[Muxer] failed to create pad";
            return pgraph::api::INVALID_PAD_ID;
        }

        d->streams[padId] = nullptr;
        return padId;
    }

    [[maybe_unused]] void Muxer::destroyStreamPad(int64_t padId) {
        Q_D(Muxer);
        if (d->streams.find(padId) != d->streams.end() && d->streams[padId] == nullptr) {
            d->streams.erase(padId);
        } else if (d->streams[padId] != nullptr) {
            qWarning() << "[Muxer] pad" << padId << "is already in use, cannot destroy";
        }
    }

    bool Muxer::isOpen() const {
        Q_D(const Muxer);
        return std::find_if(d->streams.begin(), d->streams.end(), [](const auto &stream) {
                   return stream.second == nullptr;
               }) == d->streams.end();// all streams are initialized
    }

    bool Muxer::isRunning() const {
        Q_D(const Muxer);
        return d->running;
    }

    bool Muxer::isPaused() const {
        Q_D(const Muxer);
        return d->paused;
    }

    void Muxer::run() {
        Q_D(Muxer);

        emit started();

        while (d->running) {
            if (d->paused) {
                std::unique_lock<std::mutex> lock(d->pausedMutex);
                d->pausedCond.wait(lock, [d] {
                    return !d->paused || !d->running;
                });
                if (!d->running) {
                    break;
                }
            }

            std::unique_lock<std::mutex> lock(d->inputQueueMutex);
            if (d->inputQueue.empty()) {
                d->inputQueueCondition.wait(lock, [d] {
                    return !d->inputQueue.empty() || !d->running;
                });
                if (!d->running) {
                    break;
                }
            }

            auto packet = d->inputQueue.front();

            if (!packet) {
                d->inputQueue.pop_front();
                continue;
            }

            lock.unlock();

            int ret = av_interleaved_write_frame(d->pFormatCtx.get(), packet.get());
            if (ret == AVERROR(EAGAIN)) {
                continue;
            } else if (ret < 0) {
                char strBuf[AV_ERROR_MAX_STRING_SIZE];
                qWarning() << "[Muxer] failed to write frame:" << av_make_error_string(strBuf, AV_ERROR_MAX_STRING_SIZE, ret);
                d->running = false;
                break;
            } else {
                lock.lock();
                d->inputQueue.pop_front();
            }
            d->inputQueueCondition.notify_all();
        }
    }

    void MuxerPrivate::destroyAVFormatContext(AVFormatContext *ctx) {
        if (ctx) {
            avformat_free_context(ctx);
        }
    }

    void MuxerPrivate::destroyAVIOContext(AVIOContext *ctx) {
        if (ctx) {
            if (avio_closep(&ctx) != 0) {
                qWarning() << "Error closing AVIOContext";
            }
        }
    }

    void MuxerPrivate::enqueueData(const std::shared_ptr<AVPacket> &packet) {
        std::unique_lock<std::mutex> lock(inputQueueMutex);
        if (inputQueue.size() >= inputQueueMaxSize) {
            qDebug() << "[Muxer] input queue full";
            inputQueueCondition.wait(lock, [this] { return inputQueue.size() < inputQueueMaxSize || !running; });
            if (!running) {
                return;
            }
        }
        inputQueue.push_back(packet);
        inputQueueCondition.notify_all();
    }

    int MuxerPrivate::writeIO(void *opaque, uint8_t *buf, int buf_size) {
        auto d = static_cast<MuxerPrivate *>(opaque);

        auto bytesWritten = d->outputDevice->write(reinterpret_cast<const char *>(buf), buf_size);
        return bytesWritten == 0 ? AVERROR(EIO) : static_cast<int>(bytesWritten);
    }

    int64_t MuxerPrivate::seekIO(void *opaque, int64_t offset, int whence) {
        auto d = static_cast<MuxerPrivate *>(opaque);

        if (d->outputDevice->isSequential()) {
            return AVERROR_UNKNOWN;
        }

        bool result;
        switch (whence) {
            case SEEK_SET:
                result = d->outputDevice->seek(offset);
                break;
            case SEEK_CUR:
                result = d->outputDevice->seek(d->outputDevice->pos() + offset);
                break;
            case SEEK_END:
                result = d->outputDevice->seek(d->outputDevice->size() - offset);
                break;
            case AVSEEK_SIZE:
                return d->outputDevice->size();
            default:
                return AVERROR_UNKNOWN;
        }

        return result ? d->outputDevice->pos() : AVERROR_UNKNOWN;
    }

    bool MuxerPrivate::initStream(int64_t padId, const std::shared_ptr<communication::PacketPadParams> &params) {
        Q_Q(Muxer);

        if (streams.find(padId) != streams.end() && streams[padId] != nullptr) {
            qWarning() << "[Muxer] pad" << padId << "is already initialized";
            return false;
        }

        auto stream = avformat_new_stream(pFormatCtx.get(), params->codec);
        if (!stream) {
            qWarning() << "[Muxer] failed to create new stream";
            return false;
        }

        streams[padId] = stream;

        if (std::find_if(streams.begin(), streams.end(), [](const auto &pair) { return pair.second == nullptr; }) == streams.end()) {
            qDebug() << "[Muxer] all streams initialized";
        }

        return true;
    }

    bool MuxerPrivate::startStream(int64_t padId) {
        Q_Q(Muxer);

        if (streams.find(padId) == streams.end() || streams[padId] == nullptr) {
            qWarning() << "[Muxer] pad" << padId << "is not an initialized stream";
            return false;
        }

        startedStreams.emplace(padId);
        if (startedStreams.size() == streams.size() && !running) {
            qDebug() << "[Muxer] all streams started, starting muxing";

            q->start();
        }

        return true;
    }

    void MuxerPrivate::stopStream(int64_t padId) {
        Q_Q(Muxer);

        if (streams.find(padId) == streams.end() || streams[padId] == nullptr) {
            qWarning() << "[Muxer] pad" << padId << "is not an initialized stream";
            return;
        }

        startedStreams.erase(padId);
        if (startedStreams.empty() && running) {
            qDebug() << "[Muxer] all streams stopped, stopping muxing";

            q->stop();
        }
    }
}// namespace AVQt
