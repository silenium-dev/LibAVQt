#include "decoder/DecoderDXVA2.h"

extern "C" {
#include <libavutil/rational.h>
}

#ifndef LIBAVQT_DECODERDXVA2_P_H
#define LIBAVQT_DECODERDXVA2_P_H

namespace AVQt {
    /*!
    * \private
    */
    class DecoderDXVA2Private : public QObject {
    Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::DecoderDXVA2)

    public:
        DecoderDXVA2Private(const DecoderDXVA2Private &) = delete;

        void operator=(const DecoderDXVA2Private &) = delete;

    private:

        explicit DecoderDXVA2Private(DecoderDXVA2 *q) : q_ptr(q) {};

        DecoderDXVA2 *q_ptr;

        QMutex m_inputQueueMutex{};
        QQueue<AVPacket *> m_inputQueue{};
        int64_t m_duration{0};
        AVRational m_framerate{0, 1};
        AVRational m_timebase{0, 1};

        AVCodec *m_pCodec{nullptr};
        AVCodecParameters *m_pCodecParams{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVBufferRef *m_pDeviceCtx{nullptr};

        // Callback stuff
        QMutex m_cbListMutex{};
        QList<IFrameSink *> m_cbList{};

        // Threading stuff
        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};

        friend class DecoderDXVA2;
    };
}

#endif //LIBAVQT_DECODERDXVA2_P_H
