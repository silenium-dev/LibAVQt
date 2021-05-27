//
// Created by silas on 5/24/21.
//

#include "../Muxer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/packet.h>
}

#include <QtCore>

#ifndef LIBAVQT_MUXER_P_H
#define LIBAVQT_MUXER_P_H

namespace AVQt {
    class MuxerPrivate {
    public:
        MuxerPrivate(const MuxerPrivate &) = delete;

        void operator=(const MuxerPrivate &) = delete;

    private:
        explicit MuxerPrivate(Muxer *q) : q_ptr(q) {};

        Muxer *q_ptr;

        QIODevice *m_outputDevice{nullptr};

        QMutex m_initMutex{};
        static constexpr size_t IOBUF_SIZE{4 * 1024};  // 4 KB
        uint8_t *m_pIOBuffer{nullptr};
        AVIOContext *m_pIOContext{nullptr};
        AVFormatContext *m_pFormatContext{nullptr};
        QMap<IPacketSource *, QList<AVStream *>> m_sourceStreamMap{};

        QMutex m_inputQueueMutex{};
        QQueue<QPair<AVPacket *, AVStream *>> m_inputQueue{};

        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};
        std::atomic_bool m_headerWritten{false};

        friend class Muxer;

        static int64_t seekIO(void *opaque, int64_t offset, int whence);

        static int writeToIO(void *opaque, uint8_t *buf, int buf_size);
    };
}

#endif //LIBAVQT_MUXER_P_H