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

#include "AVQt/decoder/VideoDecoder.hpp"
#include "AVQt/decoder/IVideoDecoderImpl.hpp"
#include "AVQt/communication/VideoPadParams.hpp"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

#include <pgraph_network/api/PadRegistry.hpp>


#ifndef TRANSCODE_DECODERVAAPI_P_H
#define TRANSCODE_DECODERVAAPI_P_H

namespace AVQt {
    class VideoDecoder;
    /*!
     * \private
     */
    class VideoDecoderPrivate : public QObject {
        Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::VideoDecoder)

    public:
        VideoDecoderPrivate(const VideoDecoderPrivate &) = delete;

        void operator=(const VideoDecoderPrivate &) = delete;

    private:
        explicit VideoDecoderPrivate(VideoDecoder *q) : q_ptr(q){};

        void enqueueData(const std::shared_ptr<AVPacket> &packet);

        VideoDecoder *q_ptr;

        int64_t inputPadId{pgraph::api::INVALID_PAD_ID};
        int64_t outputPadId{pgraph::api::INVALID_PAD_ID};

        QMutex inputQueueMutex{};
        QQueue<std::shared_ptr<AVPacket>> inputQueue{};

        VideoDecoder::Config config{};

        std::shared_ptr<AVCodecParameters> codecParams{};
        std::shared_ptr<api::IVideoDecoderImpl> impl{};

        std::shared_ptr<communication::VideoPadParams> outputPadParams{};

        // Threading stuff
        std::atomic_bool running{false}, paused{false}, open{false}, initialized{false};

        friend class VideoDecoder;
    };

}// namespace AVQt

#endif//TRANSCODE_DECODERVAAPI_P_H
