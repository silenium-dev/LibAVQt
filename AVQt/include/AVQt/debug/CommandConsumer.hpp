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

#ifndef LIBAVQT_COMMANDCONSUMER_H
#define LIBAVQT_COMMANDCONSUMER_H

#include "AVQt/communication/Message.hpp"
#include "pgraph/impl/SimpleConsumer.hpp"
#include "pgraph_network/api/PadRegistry.hpp"

namespace AVQt::debug {
    class CommandConsumer : public pgraph::impl::SimpleConsumer {
    public:
        CommandConsumer(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry);
        ~CommandConsumer() override = default;

        void init();

        void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) override;

    private:
        int64_t m_commandInputPadId;
        std::atomic_uint64_t m_frameCount;
        uint32_t m_id;
        static std::atomic_uint32_t m_nextId;
    };
}// namespace AVQt::debug


#endif//LIBAVQT_COMMANDCONSUMER_H
