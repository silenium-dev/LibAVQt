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

#include "communication/PacketPadParams.hpp"
#include "encoder/IEncoderImpl.hpp"
#include <QtCore>


extern "C" {
#include <libavcodec/avcodec.h>
};

namespace AVQt {
    class Encoder;
    class EncoderPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(Encoder)
    private slots:
        void onPacketReady(AVPacket *packet);

    private:
        explicit EncoderPrivate(Encoder *q) : QObject(), q_ptr(q){};

        void enqueueData(AVFrame *frame);

        Encoder *q_ptr;

        QThread *prevThread{nullptr};

        int64_t inputPadId{pgraph::api::INVALID_PAD_ID};
        int64_t outputPadId{pgraph::api::INVALID_PAD_ID};

        QMutex inputQueueMutex{};
        QQueue<AVFrame *> inputQueue{};

        EncodeParameters encodeParameters{};
        AVCodecParameters *pCodecParams{nullptr};
        api::IEncoderImpl *impl{nullptr};

        VideoPadParams inputParams{};

        std::shared_ptr<PacketPadParams> outputPadParams{};
        std::shared_ptr<PacketPadParams> inputPadParams{};

        // Threading stuff
        std::atomic_bool running{false}, paused{false}, open{false}, initialized{false};
    };
}// namespace AVQt


#endif//LIBAVQT_TRANSCODER_P_HPP
