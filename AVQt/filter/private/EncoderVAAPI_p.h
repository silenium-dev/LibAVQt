//
// Created by silas on 4/18/21.
//

#include "../EncoderVAAPI.h"

#include <QtCore>

#ifndef LIBAVQT_ENCODERVAAPI_P_H
#define LIBAVQT_ENCODERVAAPI_P_H

namespace AVQt {
    class EncoderVAAPIPrivate {
        explicit EncoderVAAPIPrivate(EncoderVAAPI *q) : q_ptr(q) {};

        EncoderVAAPI *q_ptr;

        QString m_encoder{""};

        AVRational m_framerate{0, 1};
        AVCodec *m_pCodec{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVBufferRef *m_pDeviceCtx{nullptr}, *m_pFramesCtx{nullptr};
        AVFrame *m_pHWFrame{nullptr};

        QMutex m_inputQueueMutex;
        QQueue<QPair<AVFrame *, int64_t>> m_inputQueue;

        QMutex m_cbListMutex;
        QList<IPacketSink *> m_cbList;

        std::atomic_bool m_running{false}, m_paused{false}, m_firstFrame{true};

        friend class EncoderVAAPI;
    };
}

#endif //LIBAVQT_ENCODERVAAPI_P_H