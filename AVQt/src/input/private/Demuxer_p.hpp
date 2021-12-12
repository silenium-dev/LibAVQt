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

        std::atomic_bool running{false}, paused{false}, open{false}, initialized{false};

        QList<int64_t> videoStreams{}, audioStreams{}, subtitleStreams{};

        static constexpr size_t BUFFER_SIZE{32 * 1024}; // 32KB
        uint8_t *pBuffer{nullptr};
        AVFormatContext *pFormatCtx{nullptr};
        AVIOContext *pIOCtx{nullptr};

        QMap<int64_t, quint32> outputPadIds;

        friend class Demuxer;
    };
}// namespace AVQt

#endif//LIBAVQT_DEMUXER_P_H
