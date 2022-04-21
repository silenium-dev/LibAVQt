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

#ifndef LIBAVQT_DEMUXER_H
#define LIBAVQT_DEMUXER_H

#include "AVQt/communication/IComponent.hpp"

#include <QtCore/QIODevice>
#include <QtCore/QThread>
#include <pgraph/impl/SimpleProcessor.hpp>
#include <pgraph_network/api/PadRegistry.hpp>


namespace AVQt {
    class DemuxerPrivate;

    class Demuxer : public QThread, public api::IComponent, public pgraph::impl::SimpleProducer {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IComponent)
        Q_DECLARE_PRIVATE(AVQt::Demuxer)

    public:
        struct Config {
            bool loop{false};
            std::unique_ptr<QIODevice> inputDevice{};
        };

        explicit Demuxer(Config inputDevice, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);

        explicit Demuxer(Demuxer &other) = delete;

        Demuxer &operator=(const Demuxer &other) = delete;

        ~Demuxer() noexcept override;

        bool isPaused() const override;

        bool isOpen() const override;

        bool isRunning() const override;

        Q_INVOKABLE bool init() override;

    public slots:
        Q_INVOKABLE bool open() override;

        Q_INVOKABLE void close() override;

        Q_INVOKABLE bool start() override;

        Q_INVOKABLE void stop() override;

        Q_INVOKABLE void pause(bool pause) override;

    signals:

        /*!
         * \brief Emitted when started
         */
        void started() override;

        /*!
         * \brief Emitted when stopped
         */
        void stopped() override;

        /*!
         * \brief Emitted when paused state changed
         * @param pause Current paused state
         */
        void paused(bool pause) override;

    protected:
        void run() override;

        DemuxerPrivate *d_ptr;
    };
}// namespace AVQt

#endif//LIBAVQT_DEMUXER_H
