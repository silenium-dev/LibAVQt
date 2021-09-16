//
// Created by silas on 4/28/21.
//

#include "../DecoderD3D11VA.h"

extern "C" {
#include <libavutil/rational.h>
}

#ifndef LIBAVQT_DECODERD3D11VA_P_H
#define LIBAVQT_DECODERD3D11VA_P_H

namespace AVQt {
    /*!
    * \private
    */
    class DecoderD3D11VAPrivate {
    public:
        DecoderD3D11VAPrivate(const DecoderD3D11VAPrivate &) = delete;

        void operator=(const DecoderD3D11VAPrivate &) = delete;

    private:

        explicit DecoderD3D11VAPrivate(DecoderD3D11VA *q) : q_ptr(q) {};

        DecoderD3D11VA *q_ptr;

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

        friend class DecoderD3D11VA;
    };
}

#endif //LIBAVQT_DECODERD3D11VA_P_H