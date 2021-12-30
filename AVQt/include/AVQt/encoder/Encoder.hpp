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

#ifndef LIBAVQT_ENCODER_HPP
#define LIBAVQT_ENCODER_HPP

#include "IEncoderImpl.hpp"
#include "communication/IComponent.hpp"
#include "pgraph_network/api/PadRegistry.hpp"

#include <QtCore>
#include <pgraph/impl/SimpleProcessor.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
};

namespace AVQt {
    class EncoderPrivate;
    class Encoder : public QThread, public api::IComponent, public pgraph::impl::SimpleProcessor {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IComponent)
        Q_DECLARE_PRIVATE(AVQt::Encoder)
    public:
        static AVCodecID codecId(Codec codec);

        Encoder(const QString &encoderName, const EncodeParameters &encodeParameters, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        /*!
         * \private
         */
        ~Encoder() Q_DECL_OVERRIDE;
        /*!
         * \private
         */
        Encoder(const Encoder &) = delete;
        /*!
         * \private
         */
        void operator=(const Encoder &) = delete;

        bool isOpen() const override;
        bool isRunning() const override;

        /*!
         * \brief Returns paused state of decoder
         * @return Paused state
         */
        bool isPaused() const Q_DECL_OVERRIDE;

        [[nodiscard]] int64_t getInputPadId() const;
        [[nodiscard]] int64_t getOutputPadId() const;

    public slots:
        Q_INVOKABLE bool init() Q_DECL_OVERRIDE;

        Q_INVOKABLE bool open() Q_DECL_OVERRIDE;
        Q_INVOKABLE void close() Q_DECL_OVERRIDE;

        Q_INVOKABLE bool start() Q_DECL_OVERRIDE;
        Q_INVOKABLE void stop() Q_DECL_OVERRIDE;

        Q_INVOKABLE void pause(bool pause) Q_DECL_OVERRIDE;

        void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) Q_DECL_OVERRIDE;

    signals:

        /*!
         * \brief Emitted after start() finished
         */
        void started() Q_DECL_OVERRIDE;

        /*!
         * \brief Emitted after stop() finished
         */
        void stopped() Q_DECL_OVERRIDE;

        /*!
         * \brief Emitted when paused state changes
         * @param pause Current paused state
         */
        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        void run() Q_DECL_OVERRIDE;

    private:
        QScopedPointer<EncoderPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_ENCODER_HPP
