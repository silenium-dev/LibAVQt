#include "GenericAudioEncoderImpl.hpp"
#include "private/GenericAudioEncoderImpl_p.hpp"

#include "encoder/AudioEncoderFactory.hpp"

#include <static_block.hpp>

namespace AVQt {
    const AudioEncoderInfo &GenericAudioEncoderImpl::info() {
        static AudioEncoderInfo info{
                .metaObject = GenericAudioEncoderImpl::staticMetaObject,
                .name = "Generic",
                .platforms = {
                        common::Platform::All,
                },
                .supportedInputFormats = {},
                .supportedCodecIds = {},
        };
        return info;
    }

    GenericAudioEncoderImpl::GenericAudioEncoderImpl(AVCodecID codecId, AVQt::AudioEncodeParameters encodeParameters, QObject *parent)
        : QObject(parent),
          api::IAudioEncoderImpl(encodeParameters),
          d_ptr(new GenericAudioEncoderImplPrivate(codecId, encodeParameters, this)) {
    }

    GenericAudioEncoderImplPrivate::GenericAudioEncoderImplPrivate(AVCodecID codecId, AudioEncodeParameters encodeParameters, GenericAudioEncoderImpl *q)
        : q_ptr(q) {
        this->encodeParameters = encodeParameters;
        codec = avcodec_find_encoder(codecId);
        if (codec->type != AVMEDIA_TYPE_AUDIO) {
            throw std::runtime_error("Codec is not an audio codec");
        }
    }

    GenericAudioEncoderImpl::~GenericAudioEncoderImpl() {
        Q_D(GenericAudioEncoderImpl);
        if (d->open) {
            qFatal("GenericAudioEncoderImpl::~GenericAudioEncoderImpl: Encoder is still open");
        }
    }

    bool GenericAudioEncoderImpl::open(const communication::AudioPadParams &params) {
        Q_D(GenericAudioEncoderImpl);

        bool shouldBe = false;

        if (d->open.compare_exchange_strong(shouldBe, true)) {
            d->inputParameters = params;

            d->codecContext.reset(avcodec_alloc_context3(d->codec));
            if (!d->codecContext) {
                throw std::runtime_error("Could not allocate codec context");
            }

            d->codecContext->sample_fmt = params.format.sampleFormat();
            d->codecContext->sample_rate = params.format.sampleRate();
            d->codecContext->channel_layout = params.format.channelLayout() < 0 ? av_get_default_channel_layout(params.format.channels()) : params.format.channelLayout();
            d->codecContext->channels = params.format.channels();
            d->codecContext->time_base = {1, 1000000};// Microseconds
            d->codecContext->bit_rate = d->encodeParameters.bitrate;

            int ret = avcodec_open2(d->codecContext.get(), d->codec, nullptr);
            if (ret < 0) {
                char err[AV_ERROR_MAX_STRING_SIZE];
                qWarning("Could not open codec: %s", av_make_error_string(err, AV_ERROR_MAX_STRING_SIZE, ret));
                d->open = false;
                return false;
            }

            return true;
        } else {
            return false;
        }
    }

    void GenericAudioEncoderImpl::close() {
        Q_D(GenericAudioEncoderImpl);

        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            avcodec_send_frame(d->codecContext.get(), nullptr);
            std::shared_ptr<AVPacket> pkt(av_packet_alloc(), [](AVPacket *pkt) {
                av_packet_free(&pkt);
            });
            int ret = avcodec_receive_packet(d->codecContext.get(), pkt.get());
            while (ret == 0) {
                packetReady(pkt);
                pkt.reset(av_packet_alloc());
                ret = avcodec_receive_packet(d->codecContext.get(), pkt.get());
                if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF && ret != 0) {
                    char err[AV_ERROR_MAX_STRING_SIZE];
                    qWarning("Could not receive packet: %s", av_make_error_string(err, AV_ERROR_MAX_STRING_SIZE, ret));
                    break;
                }
            }
            d->codecContext.reset();
        } else {
            qWarning("GenericAudioEncoderImpl::close: Encoder is not open");
        }
    }

    int GenericAudioEncoderImpl::encode(std::shared_ptr<AVFrame> frame) {
        Q_D(GenericAudioEncoderImpl);

        if (!d->open) {
            qWarning("GenericAudioEncoderImpl::encode: Encoder is not open");
            return ENODEV;
        }

        int ret = avcodec_send_frame(d->codecContext.get(), frame.get());
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            char err[AV_ERROR_MAX_STRING_SIZE];
            qWarning("Could not send frame: %s", av_make_error_string(err, AV_ERROR_MAX_STRING_SIZE, ret));
            return AVUNERROR(ret);
        } else if (ret == AVERROR(EAGAIN)) {
            return EAGAIN;
        } else if (ret == AVERROR_EOF) {
            return EOF;
        }

        std::shared_ptr<AVPacket> pkt(av_packet_alloc(), [](AVPacket *pkt) {
            av_packet_free(&pkt);
        });
        ret = avcodec_receive_packet(d->codecContext.get(), pkt.get());
        while (ret == 0 && pkt->pts != AV_NOPTS_VALUE) {
            if (pkt->data == nullptr) {
                qWarning("GenericAudioEncoderImpl::encode: Received empty packet");
                continue;
            }
            packetReady(pkt);
            pkt.reset(av_packet_alloc());
            ret = avcodec_receive_packet(d->codecContext.get(), pkt.get());
        }
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            char err[AV_ERROR_MAX_STRING_SIZE];
            qWarning("Could not receive packet: %s", av_make_error_string(err, AV_ERROR_MAX_STRING_SIZE, ret));
            return AVUNERROR(ret);
        }

        return EXIT_SUCCESS;
    }

    QVector<AVSampleFormat> GenericAudioEncoderImpl::getInputFormats() const {
        Q_D(const GenericAudioEncoderImpl);

        QVector<AVSampleFormat> formats;

        if (d->codecContext) {
            formats.append(d->codecContext->sample_fmt);
        } else {
            for (const auto &fmt : info().supportedInputFormats) {
                formats.append(fmt.sampleFormat());
            }
            if (formats.isEmpty()) {
                for (auto fmt = d->codec->sample_fmts; *fmt != AV_SAMPLE_FMT_NONE; ++fmt) {
                    formats.append(*fmt);
                }
            }
        }

        return formats;
    }

    std::shared_ptr<AVCodecParameters> GenericAudioEncoderImpl::getCodecParameters() const {
        Q_D(const GenericAudioEncoderImpl);
        std::shared_ptr<AVCodecParameters> params{avcodec_parameters_alloc(), &GenericAudioEncoderImplPrivate::destroyAVCodecParameters};
        avcodec_parameters_from_context(params.get(), d->codecContext.get());
        return params;
    }

    std::shared_ptr<communication::PacketPadParams> GenericAudioEncoderImpl::getPacketPadParams() const {
        Q_D(const GenericAudioEncoderImpl);
        auto params = std::make_shared<communication::PacketPadParams>();
        params->codec = d->codec;
        params->codecParams = getCodecParameters();
        params->mediaType = d->codec->type;

        return params;
    }

    void GenericAudioEncoderImplPrivate::destroyAVCodecContext(AVCodecContext *codecContext) {
        if (codecContext) {
            if (avcodec_is_open(codecContext)) {
                avcodec_close(codecContext);
            }
            avcodec_free_context(&codecContext);
        }
    }

    void GenericAudioEncoderImplPrivate::destroyAVCodecParameters(AVCodecParameters *codecParameters) {
        if (codecParameters) {
            avcodec_parameters_free(&codecParameters);
        }
    }
}// namespace AVQt

static_block {
    AVQt::AudioEncoderFactory::getInstance().registerEncoder(AVQt::GenericAudioEncoderImpl::info());
}
