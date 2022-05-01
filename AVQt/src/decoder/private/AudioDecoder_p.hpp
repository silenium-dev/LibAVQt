#ifndef LIBAVQT_AUDIODECODERPRIVATE_HPP
#define LIBAVQT_AUDIODECODERPRIVATE_HPP

#include "communication/PacketPadParams.hpp"
#include "decoder/IAudioDecoderImpl.hpp"

#include <QObject>

#include <condition_variable>
#include <mutex>
#include <queue>

namespace AVQt {
    class AudioDecoder;
    class AudioDecoderPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(AudioDecoder)
        Q_DISABLE_COPY_MOVE(AudioDecoderPrivate)
    private slots:
        void onFrame(const std::shared_ptr<AVFrame> &frame);

    private:
        explicit AudioDecoderPrivate(AudioDecoder *q);

        AudioDecoder *q_ptr;

        void init(const AudioDecoder::Config &aConfig);

        AudioDecoder::Config config;
        int64_t inputPadId{pgraph::api::INVALID_PAD_ID}, outputPadId{pgraph::api::INVALID_PAD_ID};

        std::shared_ptr<AVQt::api::IAudioDecoderImpl> impl{};
        std::shared_ptr<communication::PacketPadParams> inputPadParams{};
        std::shared_ptr<communication::AudioPadParams> outputPadParams{};
        std::shared_ptr<AVCodecParameters> codecParams{};

        std::mutex inputQueueMutex;
        std::condition_variable inputQueueCond;
        std::deque<std::shared_ptr<AVPacket>> inputQueue;

        std::mutex pausedMutex;
        std::condition_variable pausedCond;
        std::atomic_bool initialized{false}, open{false}, running{false}, paused{false};
    };
}// namespace AVQt


#endif//LIBAVQT_AUDIODECODERPRIVATE_HPP
