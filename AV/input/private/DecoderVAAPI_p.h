//
// Created by silas on 3/14/21.
//

#include "../DecoderVAAPI.h"

#ifndef TRANSCODE_DECODERVAAPI_P_H
#define TRANSCODE_DECODERVAAPI_P_H

namespace AV {
    struct DecoderVAAPIPrivate {
        explicit DecoderVAAPIPrivate(DecoderVAAPI *q) : q_ptr(q) {};

        DecoderVAAPI *q_ptr;

        static int readFromIO(void *opaque, uint8_t *buf, int bufSize);

        static int64_t seekIO(void *opaque, int64_t pos, int whence);

        uint8_t *pIOBuffer = nullptr;
        AVIOContext *pInputCtx = nullptr;
        AVFormatContext *pFormatCtx = nullptr;
        AVCodec *pVideoCodec = nullptr, *pAudioCodec = nullptr;
        AVCodecContext *pVideoCodecCtx = nullptr, *pAudioCodecCtx = nullptr;
        AVBufferRef *pDeviceCtx = nullptr;
        AVBufferRef *pFramesCtx = nullptr;
        AVFrame *pCurrentFrame = nullptr;
        AVFrame *pCurrentBGRAFrame = nullptr;

        int videoIndex = 0, audioIndex = 0;

        SwsContext *pSwsContext = nullptr;

        QIODevice *m_inputDevice = nullptr;

        // Callback stuff
        QList<IFrameSink *> m_avfCallbacks, m_qiCallbacks;

        // Threading stuff
        std::atomic_bool isRunning = false;
        std::atomic_bool isPaused = false;
    };
}

#endif //TRANSCODE_DECODERVAAPI_P_H
