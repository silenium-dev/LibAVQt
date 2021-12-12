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
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NON INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//
// Created by silas on 26.10.21.
//

#include "input/CommandConsumer.hpp"
#include "communication/PacketPadParams.hpp"
#include <pgraph_network/impl/RegisteringPadFactory.hpp>


CommandConsumer::CommandConsumer(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry)
    : pgraph::impl::SimpleConsumer(std::make_shared<pgraph::network::impl::RegisteringPadFactory>(std::move(padRegistry))) {
}

void CommandConsumer::consume(uint32_t pad, std::shared_ptr<pgraph::api::Data> data) {
    if (pad == m_commandInputPadId && data->getType() == AVQt::Message::Type) {
        auto message = std::dynamic_pointer_cast<AVQt::Message>(data);
        qDebug() << "Incoming command" << message->getAction().name() << "with payload:";
        qDebug() << message->getPayloads();
    }
}
void CommandConsumer::open() {
    m_commandInputPadId = pgraph::impl::SimpleConsumer::createInputPad(pgraph::api::PadUserData::emptyUserData());
}
