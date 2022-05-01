#include "GenericAudioDecoderImpl.hpp"
#include "private/GenericAudioDecoderImplPrivate.hpp"

#include "decoder/AudioDecoderFactory.hpp"

#include <static_block.hpp>

namespace AVQt {
    GenericAudioDecoderImpl::GenericAudioDecoderImpl(AVCodecID codecId, QObject *parent)
        : QObject(parent),
          d_ptr(new GenericAudioDecoderImplPrivate(this)) {
        Q_D(GenericAudioDecoderImpl);
        d->init(codecId);
    }

    GenericAudioDecoderImpl::~GenericAudioDecoderImpl() = default;

    bool GenericAudioDecoderImpl::open(std::shared_ptr<AVCodecParameters> codecParams) {
        Q_D(GenericAudioDecoderImpl);

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            d->codecContext.reset(avcodec_alloc_context3(d->codec));
            if (!d->codecContext) {
                throw std::runtime_error{"Could not allocate codec context"};
            }

            if (avcodec_parameters_to_context(d->codecContext.get(), codecParams.get()) < 0) {
                throw std::runtime_error{"Could not copy codec parameters to codec context"};
            }

            if (avcodec_open2(d->codecContext.get(), d->codec, nullptr) < 0) {
                throw std::runtime_error{"Could not open codec"};
            }

            d->codecParameters = codecParams;
            return true;
        } else {
            qWarning() << "GenericAudioDecoderImpl::open() called when already open";
            return false;
        }
    }

    void GenericAudioDecoderImpl::close() {
        Q_D(GenericAudioDecoderImpl);

        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            avcodec_send_packet(d->codecContext.get(), nullptr);
            std::shared_ptr<AVFrame> frame{av_frame_alloc(), &GenericAudioDecoderImplPrivate::destroyAVFrame};
            while (avcodec_receive_frame(d->codecContext.get(), frame.get()) == 0) {
                emit frameReady(frame);
                frame.reset(av_frame_alloc());
            }
            d->codecContext.reset();
        } else {
            qWarning() << "GenericAudioDecoderImpl::close() called when not open";
        }
    }

    common::AudioFormat GenericAudioDecoderImpl::getOutputFormat() const {
        Q_D(const GenericAudioDecoderImpl);
        if (d->codecParameters) {
            return {d->codecParameters->sample_rate,
                    d->codecParameters->channels,
                    static_cast<AVSampleFormat>(d->codecParameters->format),
                    d->codecParameters->channel_layout};
        }
        throw std::runtime_error{"Output format is not available"};
    }

    communication::AudioPadParams GenericAudioDecoderImpl::getAudioParams() const {
        return communication::AudioPadParams{getOutputFormat()};
    }

    int GenericAudioDecoderImpl::decode(std::shared_ptr<AVPacket> packet) {
        Q_D(GenericAudioDecoderImpl);
        if (!d->codecContext) {
            throw std::runtime_error{"Decoder is not open"};
        }

        int ret = avcodec_send_packet(d->codecContext.get(), packet.get());
        if (ret < 0) {
            throw std::runtime_error{"Could not send packet to decoder"};
        }

        std::shared_ptr<AVFrame> frame{av_frame_alloc(), &GenericAudioDecoderImplPrivate::destroyAVFrame};
        while (ret >= 0) {
            ret = avcodec_receive_frame(d->codecContext.get(), frame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                throw std::runtime_error{"Could not receive frame from decoder"};
            }
            emit frameReady(frame);
            frame.reset(av_frame_alloc());
        }
        return EXIT_SUCCESS;
    }

    const api::AudioDecoderInfo GenericAudioDecoderImpl::info() {// NOLINT(readability-const-return-type)
        static const api::AudioDecoderInfo info{
                .metaObject = staticMetaObject,
                .name = "Generic",
                .platforms = {
                        common::Platform::All,
                },
                .supportedInputAudioFormats = {
                        // Empty => all formats are supported
                },
                .supportedCodecIds = {
                        // Empty => all codecs are supported
                },
        };
        return info;
    }

    GenericAudioDecoderImplPrivate::GenericAudioDecoderImplPrivate(GenericAudioDecoderImpl *q) : q_ptr(q) {}

    void GenericAudioDecoderImplPrivate::init(const AVCodecID codecId) {
        Q_Q(GenericAudioDecoderImpl);
        codec = avcodec_find_decoder(codecId);
        if (!codec) {
            throw std::runtime_error{"Codec not found"};
        }
    }

    void GenericAudioDecoderImplPrivate::destroyAVCodecContext(AVCodecContext *codecContext) {
        if (codecContext) {
            if (avcodec_is_open(codecContext)) {
                avcodec_close(codecContext);
            }
            avcodec_free_context(&codecContext);
        }
    }

    void GenericAudioDecoderImplPrivate::destroyAVFrame(AVFrame *frame) {
        if (frame) {
            av_frame_free(&frame);
        }
    }
}// namespace AVQt

static_block {
    AVQt::AudioDecoderFactory::getInstance().registerDecoder(AVQt::GenericAudioDecoderImpl::info());
}
