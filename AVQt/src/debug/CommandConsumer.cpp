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
// Created by silas on 26.10.21.
//

#include "debug/CommandConsumer.hpp"

#include "communication/PacketPadParams.hpp"
#include "global.hpp"

#include <QImage>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>

extern "C" {
#include <libavutil/pixdesc.h>
}

std::atomic_uint32_t CommandConsumer::m_nextId{0};

CommandConsumer::CommandConsumer(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry)
    : pgraph::impl::SimpleConsumer(std::make_shared<pgraph::network::impl::RegisteringPadFactory>(std::move(padRegistry))),
      m_id(m_nextId++) {
}

void CommandConsumer::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
    if (pad == m_commandInputPadId && data->getType() == AVQt::communication::Message::Type) {
        auto message = std::dynamic_pointer_cast<AVQt::communication::Message>(data);
        qDebug() << "Incoming command" << message->getAction().name() << "with payload:";
        qDebug() << message->getPayloads();
        if (message->getPayloads().contains("frame")) {
            auto frame = message->getPayload("frame").value<std::shared_ptr<AVFrame>>();
            qDebug() << av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format));
        }
        //            auto *frame = message->getPayloads().value("frame").value<AVFrame *>();
        //            if (m_frameCount % 50 == 0) {
        //                AVFrame *swFrame = av_frame_alloc();
        //                if (0 == av_hwframe_transfer_data(swFrame, frame, 0)) {
        //                    QImage img(swFrame->data[0], frame->width, frame->height, swFrame->linesize[0], QImage::Format_Grayscale8);
        //                    img.save(QString::number(m_id) + "frame" + QString::number(m_frameCount) + ".bmp");
        //                } else {
        //                    qWarning() << "Could not transfer frame data";
        //                }
        //                av_frame_free(&swFrame);
        //            }
        //            qDebug("Received frame %lu", m_frameCount++);
        //        }
    }
}

void CommandConsumer::init() {
    m_commandInputPadId = pgraph::impl::SimpleConsumer::createInputPad(pgraph::api::PadUserData::emptyUserData());
    if (m_commandInputPadId == pgraph::api::INVALID_PAD_ID) {
        qWarning() << "Failed to create input pad";
    }
}
