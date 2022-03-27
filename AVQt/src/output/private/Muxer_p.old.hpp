// Copyright (c) 2021.
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

#include "output/Muxer.old.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <QtCore>

#ifndef LIBAVQT_MUXER_P_H
#define LIBAVQT_MUXER_P_H

namespace AVQt {
    class MuxerPrivate : public QObject {
        Q_DECLARE_PUBLIC(AVQt::Muxer)

    public:
        MuxerPrivate(const MuxerPrivate &) = delete;

        void operator=(const MuxerPrivate &) = delete;

    private:
        explicit MuxerPrivate(Muxer *q) : q_ptr(q){};

        Muxer *q_ptr;

        QIODevice *m_outputDevice{nullptr};

        Muxer::FORMAT m_format{Muxer::FORMAT::INVALID};

        QMutex m_initMutex{}, m_ioMutex{};
        static constexpr int64_t IOBUF_SIZE{4 * 1024};// 4 KB
        uint8_t *m_pIOBuffer{nullptr};
        AVIOContext *m_pIOContext{nullptr};
        AVFormatContext *m_pFormatContext{nullptr};
        QMap<IPacketSource *, QMap<AVStream *, AVRational>> m_sourceStreamMap{};

        QMutex m_inputQueueMutex{};
        QQueue<QPair<AVPacket *, AVStream *>> m_inputQueue{};

        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};
        std::atomic_bool m_headerWritten{false};

        friend class Muxer;

        static int64_t seekIO(void *opaque, int64_t offset, int whence);

        static int writeToIO(void *opaque, uint8_t *buf, int buf_size);

        static bool packetQueueCompare(const QPair<AVPacket *, AVStream *> &packet1, const QPair<AVPacket *, AVStream *> &packet2);
    };
}// namespace AVQt

#endif//LIBAVQT_MUXER_P_H
