/*!
 * \private
 * \internal
 */

#include "../DecoderVAAPI.h"

extern "C" {
#include <libavutil/rational.h>
#include <libavutil/frame.h>
}


#ifndef TRANSCODE_DECODERVAAPI_P_H
#define TRANSCODE_DECODERVAAPI_P_H

namespace AVQt {
    /*!
     * \private
     */
    class DecoderVAAPIPrivate : public QObject {
    Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::DecoderVAAPI)

    public:
        DecoderVAAPIPrivate(const DecoderVAAPIPrivate &) = delete;

        void operator=(const DecoderVAAPIPrivate &) = delete;

    private:
        explicit DecoderVAAPIPrivate(DecoderVAAPI *q) : q_ptr(q) {};

        DecoderVAAPI *q_ptr;

        QMutex m_inputQueueMutex{};
        QQueue<AVPacket *> m_inputQueue{};
        int64_t m_duration{0};
        AVRational m_framerate{};
        AVRational m_timebase{};

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

        friend class DecoderVAAPI;
    };
}

#endif //TRANSCODE_DECODERVAAPI_P_H