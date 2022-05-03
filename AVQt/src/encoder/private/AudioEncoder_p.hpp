#ifndef LIBAVQT_AUDIOENCODER_P_HPP
#define LIBAVQT_AUDIOENCODER_P_HPP

#include "encoder/AudioEncoder.hpp"
#include "encoder/IAudioEncoderImpl.hpp"

#include <QObject>

#include <condition_variable>
#include <mutex>
#include <queue>

extern "C" {
#include <libavutil/frame.h>
};

namespace AVQt {
    class AudioEncoder;
    class AudioEncoderPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(AudioEncoder)
    private:
        AudioEncoderPrivate(AudioEncoder::Config config, AudioEncoder *q);
        AudioEncoder *q_ptr;

        void enqueueData(std::shared_ptr<AVFrame> frame);

        AudioEncoder::Config config;

        int64_t inputPadId{pgraph::api::INVALID_PAD_ID};
        int64_t outputPadId{pgraph::api::INVALID_PAD_ID};

        std::shared_ptr<communication::AudioPadParams> inputPadParams;
        std::shared_ptr<communication::PacketPadParams> outputPadParams;

        std::shared_ptr<AVCodecParameters> codecParams;
        communication::AudioPadParams inputParams;

        std::shared_ptr<api::IAudioEncoderImpl> impl{};

        static constexpr auto inputQueueMaxSize = 64;
        std::mutex inputQueueMutex{};
        std::condition_variable inputQueueCond{};
        std::queue<std::shared_ptr<AVFrame>> inputQueue{};

        std::mutex pausedMutex;
        std::condition_variable pausedCond;
        std::atomic_bool initialized{false}, open{false}, running{false}, paused{false};
    };
}// namespace AVQt


#endif//LIBAVQT_AUDIOENCODER_P_HPP
