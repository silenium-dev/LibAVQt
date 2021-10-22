#include "../EncoderVAAPI.h"

#include <QtCore>

#ifndef LIBAVQT_ENCODERVAAPI_P_H
#define LIBAVQT_ENCODERVAAPI_P_H

namespace AVQt {
    class EncoderVAAPIPrivate {
    public:
        EncoderVAAPIPrivate(const EncoderVAAPIPrivate &) = delete;

        void operator=(const EncoderVAAPIPrivate &) = delete;

    private:
        explicit EncoderVAAPIPrivate(EncoderVAAPI *q) : q_ptr(q) {};

        EncoderVAAPI *q_ptr;

        IEncoder::CODEC m_codec{IEncoder::CODEC::H264};
        int m_bitrate{5 * 1024 * 1024};
        qint64 m_duration{};

        AVCodec *m_pCodec{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVBufferRef *m_pDeviceCtx{nullptr}, *m_pFramesCtx{nullptr};
        AVFrame *m_pHWFrame{nullptr};

        QMutex m_inputQueueMutex{};
        QQueue<QPair<AVFrame *, int64_t>> m_inputQueue{};

        QMutex m_cbListMutex{};
        QList<IPacketSink *> m_cbList{};

        std::atomic_bool m_running{false}, m_paused{false}, m_firstFrame{true};

        friend class EncoderVAAPI;
    };
}

#endif //LIBAVQT_ENCODERVAAPI_P_H
