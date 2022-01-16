// Copyright (c) 2021-2022.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

/*!
 * \private
 * \internal
 */

#include "decoder/Decoder.hpp"
#include "decoder/IDecoderImpl.hpp"
#include "communication/VideoPadParams.hpp"

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

        void enqueueData(AVPacket *&packet);

        Decoder *q_ptr;

        int64_t inputPadId{pgraph::api::INVALID_PAD_ID};
        int64_t outputPadId{pgraph::api::INVALID_PAD_ID};

        QMutex inputQueueMutex{};
        QQueue<AVPacket *> inputQueue{};

        AVCodecParameters *pCodecParams{nullptr};
        std::shared_ptr<api::IDecoderImpl> impl{nullptr};

        std::shared_ptr<VideoPadParams> outputPadParams{};

        // Threading stuff
        std::atomic_bool running{false}, paused{false}, open{false}, initialized{false};

        friend class Decoder;
    };

}// namespace AVQt

#endif//TRANSCODE_DECODERVAAPI_P_H
