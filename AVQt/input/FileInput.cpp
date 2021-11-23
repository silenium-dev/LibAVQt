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

#include "FileInput.hpp"
#include "private/FileInput_p.hpp"

#include <communication/FilePadParams.hpp>
#include <communication/Message.h>
#include <utility>

namespace AVQt {
    FileInput::FileInput(const QString &fileName, std::shared_ptr<pgraph::api::PadFactory> padFactory, QObject *parent)
        : QThread(parent), pgraph::impl::SimpleProducer(std::move(padFactory)), d_ptr(new FileInputPrivate(this)) {
        Q_D(AVQt::FileInput);
        d->filename = fileName;
    }
    FileInput::~FileInput() {
        Q_D(AVQt::FileInput);
        FileInput::stop();
        FileInput::close();
    }

    bool FileInput::open() {
        Q_D(AVQt::FileInput);
        if (d->inputFile) {
            if (d->inputFile->isOpen()) {
                return true;
            }
        }

        d->inputFile = std::make_unique<QFile>(d->filename);
        d->inputFile->setFileName(d->filename);
        if (!d->inputFile->open(QIODevice::ReadOnly)) {
            qWarning() << "Could not open file" << d->filename;
            return false;
        }
        d->inputFile->seek(0);

        d->m_outputPadId = pgraph::impl::SimpleProducer::createOutputPad(std::make_shared<FilePadParams>());
        pgraph::impl::SimpleProducer::produce(Message::builder().withAction(Message::Action::INIT).build(), d->m_outputPadId);

        return true;
    }
    void FileInput::close() {
        Q_D(AVQt::FileInput);
        if (d->m_running) {
            stop();
        }

        pgraph::impl::SimpleProducer::produce(Message::builder().withAction(Message::Action::CLEANUP).build(), d->m_outputPadId);

        if (d->inputFile->isOpen()) {
            d->inputFile->close();
            d->inputFile.reset();
        }
        pgraph::impl::SimpleProducer::destroyOutputPad(d->m_outputPadId);
    }
    bool FileInput::start() {
        Q_D(AVQt::FileInput);
        if (!d->inputFile->isOpen()) {
            qWarning() << "File not open";
            return false;
        }

        d->inputFile->seek(0);

        bool shouldBe = false;
        if (d->m_running.compare_exchange_strong(shouldBe, true)) {
            pgraph::impl::SimpleProducer::produce(Message::builder().withAction(Message::Action::INIT).build(), d->m_outputPadId);
            QThread::start();
            return true;
        }
        return false;
    }
    void FileInput::stop() {
        Q_D(AVQt::FileInput);

        bool shouldBe = true;
        if (d->m_running.compare_exchange_strong(shouldBe, false)) {
            QThread::quit();
            QThread::wait();
        }
    }
    void FileInput::pause(bool state) {
        Q_D(AVQt::FileInput);
        bool shouldBe = !state;
        if (d->m_paused.compare_exchange_strong(shouldBe, state)) {
            paused(state);
        }
    }
    bool FileInput::isPaused() {
        Q_D(AVQt::FileInput);
        return d->m_paused.load();
    }
    void FileInput::run() {
        Q_D(AVQt::FileInput);
        pgraph::impl::SimpleProducer::produce(Message::builder().withAction(Message::Action::START).build(), d->m_outputPadId);
        while (d->m_running) {
            if (d->m_paused) {
                QThread::msleep(10);
                continue;
            }
            auto data = d->inputFile->read(1024);
            if (data.size() == 0) {
                QThread::msleep(10);
                continue;
            }

            pgraph::impl::SimpleProducer::produce(Message::builder().withAction(Message::Action::DATA).withPayload("data", data).build(), d->m_outputPadId);
        }
        pgraph::impl::SimpleProducer::produce(Message::builder().withAction(Message::Action::STOP).build(), d->m_outputPadId);
    }

    uint32_t FileInput::getOutputPadId() const {
        Q_D(const AVQt::FileInput);
        if (d->inputFile->isOpen()) {
            return d->m_outputPadId;
        } else {
            throw std::runtime_error("File not opened");
        }
    }
}// namespace AVQt
