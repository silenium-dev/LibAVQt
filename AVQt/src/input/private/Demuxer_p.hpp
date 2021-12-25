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

#include "input/Demuxer.hpp"
#include "communication/Message.hpp"

#include <QtCore>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#ifndef LIBAVQT_DEMUXER_P_H
#define LIBAVQT_DEMUXER_P_H

namespace AVQt {
    class DemuxerPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(AVQt::Demuxer)

    public:
        DemuxerPrivate(const DemuxerPrivate &) = delete;

        void operator=(const DemuxerPrivate &) = delete;

    private:
        explicit DemuxerPrivate(Demuxer *q) : q_ptr(q){};

        static int readFromIO(void *opaque, uint8_t *buf, int bufSize);

        static int64_t seekIO(void *opaque, int64_t pos, int whence);

        Demuxer *q_ptr;

        QIODevice *inputDevice{nullptr};

        QMutex mutex;
//        QWaitCondition pausedCond;
        std::atomic_bool running{false}, paused{false}, open{false}, initialized{false};

        QList<int64_t> videoStreams{}, audioStreams{}, subtitleStreams{};

        static constexpr size_t BUFFER_SIZE{32 * 1024}; // 32KB
        uint8_t *pBuffer{nullptr};
        AVFormatContext *pFormatCtx{nullptr};
        AVIOContext *pIOCtx{nullptr};

        QMap<int64_t, int64_t> outputPadIds;

        friend class Demuxer;
    };
}// namespace AVQt

#endif//LIBAVQT_DEMUXER_P_H
