// Copyright (c) 2022.
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
// Created by silas on 13.01.22.
//

#ifndef LIBAVQT_TRANSCODER_P_HPP
#define LIBAVQT_TRANSCODER_P_HPP

#include "communication/PacketPadParams.hpp"
#include "communication/VideoPadParams.hpp"
#include "decoder/IDecoderImpl.hpp"
#include "encoder/IEncoderImpl.hpp"
#include "global.hpp"
#include "transcoder/ITranscoderImpl.hpp"

#include <QObject>

namespace AVQt {
    class Transcoder;
    class TranscoderPrivate {
        Q_DECLARE_PUBLIC(Transcoder)
    private:
        explicit TranscoderPrivate(Transcoder *q);

        Transcoder *q_ptr;

        EncodeParameters encodeParameters{};

        std::shared_ptr<api::IEncoderImpl> encoderImpl{};
        std::shared_ptr<api::IDecoderImpl> decoderImpl{};

        QMutex inputQueueMutex{};
        QQueue<AVPacket *> inputQueue{};

        std::atomic_bool running{false}, paused{false}, opened{false}, initialized{false};

        int64_t inputPadId{}, frameOutputPadId{}, packetOutputPadId{};
        std::shared_ptr<VideoPadParams> videoOutputPadParams{};
        std::shared_ptr<PacketPadParams> packetOutputPadParams{};
        std::shared_ptr<PacketPadParams> inputParams{};
    };
}// namespace AVQt

#endif//LIBAVQT_TRANSCODER_P_HPP
