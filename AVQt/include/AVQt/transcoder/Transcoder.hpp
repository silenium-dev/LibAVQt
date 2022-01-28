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

#ifndef LIBAVQT_TRANSCODER_HPP
#define LIBAVQT_TRANSCODER_HPP

#include "AVQt/communication/IComponent.hpp"
#include "global.hpp"
#include <QtCore>
#include <pgraph/impl/SimpleProcessor.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

namespace AVQt {
    class TranscoderPrivate;
    class Transcoder : public QThread, public api::IComponent, public pgraph::impl::SimpleProcessor {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IComponent)
        Q_DECLARE_PRIVATE(Transcoder)
    public:
        explicit Transcoder(const QString &codecType, EncodeParameters params, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        ~Transcoder() override;

        [[nodiscard]] bool isOpen() const override;
        [[nodiscard]] bool isPaused() const override;
        [[nodiscard]] bool isRunning() const override;

        void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) override;

        int64_t getInputPadId() const;
        int64_t getFrameOutputPadId() const;
        int64_t getPacketOutputPadId() const;

    public slots:
        bool open() override;
        void close() override;
        bool init() override;
        bool start() override;
        void stop() override;
        void pause(bool state) override;

    signals:
        void started() override;
        void stopped() override;
        void paused(bool state) override;

    protected:
        void run() override;

    private slots:
        void onFrameReady(const std::shared_ptr<AVFrame> &frame);
        void onPacketReady(const std::shared_ptr<AVPacket> &packet);

    private:
        TranscoderPrivate *d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_TRANSCODER_HPP
