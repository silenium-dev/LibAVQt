//
// Created by silas on 27/03/2022.
//

#ifndef LIBAVQT_MUXERPRIVATE_HPP
#define LIBAVQT_MUXERPRIVATE_HPP

#include <QObject>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <set>

#include <pgraph/api/Pad.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
};

namespace AVQt {
    class Muxer;
    class MuxerPrivate {
        Q_DECLARE_PUBLIC(Muxer)
    public:
        static void destroyAVFormatContext(AVFormatContext *ctx);
        static void destroyAVIOContext(AVIOContext *ctx);

    private:
        explicit MuxerPrivate(Muxer *q) : q_ptr(q) {}

        static int writeIO(void *opaque, uint8_t *buf, int buf_size);
        static int64_t seekIO(void *opaque, int64_t offset, int whence);

        void init(AVQt::Muxer::Config config);

        bool initStream(int64_t padId, const std::shared_ptr<const communication::PacketPadParams> &params);
        void closeStream(int64_t padId);

        bool startStream(int64_t padId);
        void stopStream(int64_t padId);

        void enqueueData(const std::shared_ptr<AVPacket> &packet);

        constexpr static auto inputQueueMaxSize = 32;
        std::mutex inputQueueMutex{};
        std::condition_variable inputQueueCondition{};
        std::deque<std::shared_ptr<AVPacket>> inputQueue{};

        std::mutex pausedMutex{};
        std::condition_variable pausedCond{};
        std::atomic_bool running{false}, paused{false}, open{false}, initialized{false};

        std::map<int64_t, AVStream *> streams{};

        static constexpr size_t BUFFER_SIZE{32 * 1024};// 32KB
        uint8_t *pBuffer{nullptr};

        std::unique_ptr<QIODevice> outputDevice{};
        std::unique_ptr<AVFormatContext, decltype(&destroyAVFormatContext)> pFormatCtx{nullptr, &destroyAVFormatContext};
        std::unique_ptr<AVIOContext, decltype(&destroyAVIOContext)> pIOCtx{nullptr, &destroyAVIOContext};
        const AVOutputFormat *pOutputFormat{nullptr};

        std::set<int64_t> startedStreams{}, closedStreams{};

        Muxer *q_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_MUXERPRIVATE_HPP
