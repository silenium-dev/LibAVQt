/*!
 * \private
 * \internal
 */


extern "C" {
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

#include <pgraph_network/api/PadRegistry.hpp>
#include "decoder/DecoderVAAPI.h"


#ifndef TRANSCODE_DECODERVAAPI_P_H
#define TRANSCODE_DECODERVAAPI_P_H

namespace AVQt {
    class DecoderVAAPI;
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
        explicit DecoderVAAPIPrivate(DecoderVAAPI *q) : q_ptr(q){};

        static AVPixelFormat getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts);

        DecoderVAAPI *q_ptr;

        std::shared_ptr<pgraph::network::api::PadRegistry> m_padRegistry;

        uint32_t m_inputPadId{};
        uint32_t m_commandOutputPadId{};

        QMutex m_inputQueueMutex{};
        QQueue<AVPacket *> m_inputQueue{};
        int64_t m_duration{0};
        AVRational m_framerate{};
        AVRational m_timebase{};

        AVCodec *m_pCodec{nullptr};
        AVCodecParameters *m_pCodecParams{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVBufferRef *m_pDeviceCtx{nullptr};
//
//        // Callback stuff
//        QMutex m_cbListMutex{};
//        QList<IFrameSink *> m_cbList{};

        // Threading stuff
        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};

        friend class DecoderVAAPI;
    };

}// namespace AVQt

#endif//TRANSCODE_DECODERVAAPI_P_H
