/*!
 * \private
 * \internal
 */

#include "../DecoderQSV.h"

extern "C" {
#include <libavutil/rational.h>
}


#ifndef TRANSCODE_DECODERQSV_P_H
#define TRANSCODE_DECODERQSV_P_H

namespace AVQt {
    /*!
     * \private
     */
    class DecoderQSVPrivate {
        explicit DecoderQSVPrivate(DecoderQSV *q) : q_ptr(q) {};

        DecoderQSV *q_ptr;

        QMutex m_inputQueueMutex;
        QQueue<AVPacket *> m_inputQueue;
        int64_t m_duration{0};
        AVRational m_framerate;
        AVRational m_timebase;

        AVCodec *m_pCodec{nullptr};
        AVCodecParameters *m_pCodecParams{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVBufferRef *m_pDeviceCtx{nullptr};

        // Callback stuff
        QMutex m_cbListMutex;
        QList<IFrameSink *> m_cbList;

        // Threading stuff
        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};

        friend class DecoderQSV;
    };
}

#endif //TRANSCODE_DECODERQSV_P_H