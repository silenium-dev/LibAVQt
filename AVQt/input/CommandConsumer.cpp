// Copyright (c) 2021.
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
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BELIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORTOR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OROTHER DEALINGS IN THE SOFTWARE.

//
// Created by silas on 26.10.21.
//

#include "CommandConsumer.h"

#include "communication/PacketPadParams.h"


CommandConsumer::CommandConsumer() : ProcessingGraph::Consumer(false) {
    m_commandInputPadId = createPad<AVQt::Message>(QVariant::fromValue(AVQt::PacketPadParams{AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_NONE, 0}));
}

void CommandConsumer::consume(QVariant data, quint32 padId) {
    if (padId == m_commandInputPadId) {
        qDebug() << "Incoming command" << data.value<AVQt::Message>().getType().name() << "with payload:";
        qDebug() << data.value<AVQt::Message>().getPayloads();
    }
}
