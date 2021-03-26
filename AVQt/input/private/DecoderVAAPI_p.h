/*!
 * \private
 * \internal
 */

#include "../DecoderVAAPI.h"

extern "C" {
#include <libavutil/rational.h>
}


#ifndef TRANSCODE_DECODERVAAPI_P_H
#define TRANSCODE_DECODERVAAPI_P_H

namespace AVQt {
    /*!
     * \private
     */
    class DecoderVAAPIPrivate {
        explicit DecoderVAAPIPrivate(DecoderVAAPI *q) : q_ptr(q) {};

        DecoderVAAPI *q_ptr;

        QMutex m_inputQueueMutex;
        QQueue<AVPacket *> m_inputQueue;
        AVCodecParameters *m_pCodecParams = nullptr;
        int64_t m_duration = 0;
        AVRational m_framerate;

        AVCodec *m_pVideoCodec = nullptr;
        AVCodecContext *m_pVideoCodecCtx = nullptr;
        AVBufferRef *m_pDeviceCtx = nullptr;
        AVFrame *m_pCurrentVideoFrame = nullptr;

        // Callback stuff
        QMutex m_cbListMutex;
        QList<IFrameSink *> m_cbList;

        // Threading stuff
        std::atomic_bool m_running = false;
        std::atomic_bool m_paused = false;

        friend class DecoderVAAPI;
    };
}

#endif //TRANSCODE_DECODERVAAPI_P_H
