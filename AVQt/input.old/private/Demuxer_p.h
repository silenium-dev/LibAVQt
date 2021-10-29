#include "../Demuxer.h"

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

        std::atomic_bool m_running{false}, m_paused{false}, m_initialized{false};
        QMutex m_cbMutex{};
        QMap<IPacketSink *, int8_t> m_cbMap{};

        QList<int64_t> m_videoStreams{}, m_audioStreams{}, m_subtitleStreams{};
        int64_t m_videoStream{-1}, m_audioStream{-1}, m_subtitleStream{-1};

        static constexpr size_t BUFFER_SIZE{32 * 1024};
        uint8_t *m_pBuffer{nullptr};
        AVFormatContext *m_pFormatCtx{nullptr};
        AVIOContext *m_pIOCtx{nullptr};
        QIODevice *m_inputDevice{nullptr};

        friend class Demuxer;
    };
}// namespace AVQt

#endif//LIBAVQT_DEMUXER_P_H
