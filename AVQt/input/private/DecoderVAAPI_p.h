/*!
 * \private
 * \internal
 */

#include "../DecoderVAAPI.h"

#ifndef TRANSCODE_DECODERVAAPI_P_H
#define TRANSCODE_DECODERVAAPI_P_H

namespace AVQt {
    /*!
     * \private
     */
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
        AVFrame *m_pCurrentVideoFrame = nullptr;
        AVFrame *m_pCurrentAudioFrame = nullptr;
        AVFrame *m_pCurrentBGRAFrame = nullptr;

        int m_videoIndex = -1, m_audioIndex = -1;

        SwsContext *m_pSwsContext = nullptr;

        QIODevice *m_inputDevice = nullptr;

        // Callback stuff
        QMutex m_avfMutex, m_qiMutex, m_audioMutex;
        QList<IFrameSink *> m_avfCallbacks, m_qiCallbacks;
        QList<IAudioSink *> m_audioCallbacks;

        // Threading stuff
        std::atomic_bool m_isRunning = false;
        std::atomic_bool m_isPaused = false;
    };
}

#endif //TRANSCODE_DECODERVAAPI_P_H
