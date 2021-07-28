//
// Created by silas on 4/18/21.
//

#include "../EncoderQSV.h"

#include <QtCore>

#ifndef LIBAVQT_ENCODERQSV_P_H
#define LIBAVQT_ENCODERQSV_P_H

namespace AVQt {
    class EncoderQSVPrivate {
    public:
        EncoderQSVPrivate(const EncoderQSVPrivate &) = delete;

        void operator=(const EncoderQSVPrivate &) = delete;

    private:
        explicit EncoderQSVPrivate(EncoderQSV *q) : q_ptr(q) {};

        EncoderQSV *q_ptr;

        IEncoder::CODEC m_codec{IEncoder::CODEC::H264};
        int m_bitrate{5 * 1024 * 1024};

        AVRational m_framerate{0, 1}; // TODO: Remove d->m_framerate
        AVCodec *m_pCodec{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVBufferRef *m_pDeviceCtx{nullptr}, *m_pFramesCtx{nullptr};
        static constexpr auto HW_FRAME_POOL_SIZE = 4;
        AVFrame *m_pHWFrame{nullptr};
        QMutex m_codecMutex{};

        QMutex m_inputQueueMutex{};
        QQueue<QPair<AVFrame *, int64_t>> m_inputQueue{};

        IFrameSource *m_pLockedSource = nullptr;
        QMutex m_cbListMutex{};
        QList<IPacketSink *> m_cbList{};

        QMutex m_onFrameMutex{};

        std::atomic_bool m_running{false}, m_paused{false}, m_firstFrame{true}, m_initialized{false}, m_codecBufferFull{false};

        friend class EncoderQSV;
    };
}

#endif //LIBAVQT_ENCODERQSV_P_H