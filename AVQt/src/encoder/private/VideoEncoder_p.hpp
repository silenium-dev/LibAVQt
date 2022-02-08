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

//
// Created by silas on 28.12.21.
//

#ifndef LIBAVQT_TRANSCODER_P_HPP
#define LIBAVQT_TRANSCODER_P_HPP

#include "AVQt/communication/PacketPadParams.hpp"
#include "AVQt/encoder/IVideoEncoderImpl.hpp"
#include <QtCore>


extern "C" {
#include <libavcodec/avcodec.h>
};

namespace AVQt {
    class VideoEncoder;
    class VideoEncoderPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(VideoEncoder)

    private:
        explicit VideoEncoderPrivate(VideoEncoder *q) : QObject(), q_ptr(q){};

        void enqueueData(const std::shared_ptr<AVFrame> &frame);

        VideoEncoder *q_ptr;

        int64_t inputPadId{pgraph::api::INVALID_PAD_ID};
        int64_t outputPadId{pgraph::api::INVALID_PAD_ID};

        VideoEncoder::Config config;

        std::shared_ptr<AVCodecParameters> codecParams{};
        std::shared_ptr<api::IVideoEncoderImpl> impl{};

        communication::VideoPadParams inputParams{};

        std::shared_ptr<communication::PacketPadParams> outputPadParams{};
        std::shared_ptr<communication::PacketPadParams> inputPadParams{};

        // Threading stuff
        std::atomic_bool running{false}, paused{false}, open{false}, initialized{false};
    };
}// namespace AVQt


#endif//LIBAVQT_TRANSCODER_P_HPP
