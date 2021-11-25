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
// Created by silas on 23.11.21.
//

#ifndef LIBAVQT_FILEINPUT_HPP
#define LIBAVQT_FILEINPUT_HPP

#include "IInput.hpp"
#include <QtCore>
#include <pgraph/impl/SimpleProducer.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

namespace AVQt {
    class FileInputPrivate;
    class FileInput : public QThread, public pgraph::impl::SimpleProducer, public IInput {
        Q_OBJECT
        Q_DECLARE_PRIVATE(AVQt::FileInput)
    public:
        FileInput(const QString &fileName, std::shared_ptr<pgraph::api::PadFactory> padFactory, QObject *parent = nullptr);
        ~FileInput() override;

        bool waitForStarted(int msecs = 30000);

        bool open() override;
        void close() override;

        bool start() override;
        void stop() override;

        void pause(bool state) override;
        bool isPaused() override;

        uint32_t getCommandOutputPadId() const override;

    signals:
        void started() override;
        void stopped() override;
        void paused(bool state) override;

    protected:
        void run() override;

    private:
        std::unique_ptr<FileInputPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_FILEINPUT_HPP
