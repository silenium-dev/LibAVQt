/*!
 * \private
 * \internal
 */

#include "../DecoderMMAL.h"

extern "C" {
#include <libavutil/rational.h>
#include <libavutil/frame.h>
}


#ifndef TRANSCODE_DECODERMMAL_P_H
#define TRANSCODE_DECODERMMAL_P_H

namespace AVQt {
    /*!
     * \private
     */
    class DecoderMMALPrivate : public QObject {
    Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::DecoderMMAL)

    public:
        DecoderMMALPrivate(const DecoderMMALPrivate &) = delete;

        void operator=(const DecoderMMALPrivate &) = delete;

    private:
        explicit DecoderMMALPrivate(DecoderMMAL *q) : q_ptr(q) {};

        DecoderMMAL *q_ptr;

        QMutex m_inputQueueMutex{};
        QQueue<AVPacket *> m_inputQueue{};
        int64_t m_duration{0};
        AVRational m_framerate{};
        AVRational m_timebase{};

        AVCodec *m_pCodec{nullptr};
        AVCodecParameters *m_pCodecParams{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};

        // Callback stuff
        QMutex m_cbListMutex{};
        QList<IFrameSink *> m_cbList{};

        // Threading stuff
        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};

        friend class DecoderMMAL;
    };
}

#endif //TRANSCODE_DECODERMMAL_P_H