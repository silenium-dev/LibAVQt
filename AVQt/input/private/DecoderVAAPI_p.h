/*!
 * \private
 */

#include "../DecoderVAAPI.h"

#ifndef TRANSCODE_DECODERVAAPI_P_H
#define TRANSCODE_DECODERVAAPI_P_H

namespace AVQt {
    struct DecoderVAAPIPrivate {
        explicit DecoderVAAPIPrivate(DecoderVAAPI *q) : q_ptr(q) {};

        DecoderVAAPI *q_ptr;

        static int readFromIO(void *opaque, uint8_t *buf, int bufSize);

        static int64_t seekIO(void *opaque, int64_t pos, int whence);

        uint8_t *m_pIOBuffer = nullptr;
        AVIOContext *m_pInputCtx = nullptr;
        AVFormatContext *m_pFormatCtx = nullptr;
        AVCodec *m_pAudioCodec = nullptr;
        AVCodec *m_pVideoCodec = nullptr;
        AVCodecContext *m_pAudioCodecCtx = nullptr;
        AVCodecContext *m_pVideoCodecCtx = nullptr;
        AVBufferRef *m_pDeviceCtx = nullptr;
        AVFrame *m_pCurrentFrame = nullptr;
        AVFrame *m_pCurrentBGRAFrame = nullptr;

        int m_videoIndex = 0, m_audioIndex = 0;

        SwsContext *m_pSwsContext = nullptr;

        QIODevice *m_inputDevice = nullptr;

        // Callback stuff
        QList<IFrameSink *> m_avfCallbacks, m_qiCallbacks;

        // Threading stuff
        std::atomic_bool m_isRunning = false;
        std::atomic_bool m_isPaused = false;
    };
}

#endif //TRANSCODE_DECODERVAAPI_P_H
