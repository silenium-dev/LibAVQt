/*!
 * \private
 * \internal
 */

#include "decoder/Decoder.hpp"
#include "decoder/IDecoderImpl.hpp"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

#include <pgraph_network/api/PadRegistry.hpp>


#ifndef TRANSCODE_DECODERVAAPI_P_H
#define TRANSCODE_DECODERVAAPI_P_H

namespace AVQt {
    class Decoder;
    /*!
     * \private
     */
    class DecoderPrivate : public QObject {
        Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::Decoder)

    public:
        DecoderPrivate(const DecoderPrivate &) = delete;

        void operator=(const DecoderPrivate &) = delete;

    private:
        explicit DecoderPrivate(Decoder *q) : q_ptr(q){};

        static AVPixelFormat getFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts);

        void enqueueData(AVPacket *&packet);

        Decoder *q_ptr;

        std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry;

        uint32_t inputPadId{};
        uint32_t outputPadId{};

        QMutex inputQueueMutex{};
        QQueue<AVPacket *> inputQueue{};
        int64_t duration{0};

        AVCodecParameters *pCodecParams{nullptr};
        api::IDecoderImpl *impl{nullptr};

//        // Callback stuff
//        QMutex cbListMutex{};
//        QList<IFrameSink *> cbList{};

        // Threading stuff
        std::atomic_bool running{false}, paused{false}, open{false}, initialized{false};

        friend class Decoder;
    };

}// namespace AVQt

#endif//TRANSCODE_DECODERVAAPI_P_H
