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

#ifndef LIBAVQT_VIDEOENCODER_HPP
#define LIBAVQT_VIDEOENCODER_HPP

#include "IVideoEncoderImpl.hpp"
#include "AVQt/communication/IComponent.hpp"
#include "pgraph_network/api/PadRegistry.hpp"

#include <QtCore>
#include <pgraph/impl/SimpleProcessor.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
};

namespace AVQt {
    class VideoEncoderPrivate;
    class VideoEncoder : public QThread, public api::IComponent, public pgraph::impl::SimpleProcessor {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IComponent)
        Q_DECLARE_PRIVATE(AVQt::VideoEncoder)
    public:
        struct Config {
            QStringList encoderPriority{};
            Codec codec{};
            EncodeParameters encodeParameters{};
        };

        VideoEncoder(const Config &config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        VideoEncoder(const Config &config, QObject *parent = nullptr);

        /*!
         * \private
         */
        ~VideoEncoder() Q_DECL_OVERRIDE;
        /*!
         * \private
         */
        VideoEncoder(const VideoEncoder &) = delete;
        /*!
         * \private
         */
        void operator=(const VideoEncoder &) = delete;

        /*!
         * \brief Returns paused state of decoder
         * @return Paused state
         */
        bool isPaused() const Q_DECL_OVERRIDE;
        bool isOpen() const Q_DECL_OVERRIDE;
        bool isRunning() const Q_DECL_OVERRIDE;

        [[maybe_unused]] [[maybe_unused]] [[nodiscard]] int64_t getInputPadId() const;
        [[maybe_unused]] [[nodiscard]] int64_t getOutputPadId() const;

        bool init() Q_DECL_OVERRIDE;

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

    protected slots:
        bool open() Q_DECL_OVERRIDE;
        void close() Q_DECL_OVERRIDE;

        bool start() Q_DECL_OVERRIDE;
        void stop() Q_DECL_OVERRIDE;

        void pause(bool pause) Q_DECL_OVERRIDE;

        void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) Q_DECL_OVERRIDE;

    protected:
        void run() Q_DECL_OVERRIDE;

    private slots:
        void onPacketReady(const std::shared_ptr<AVPacket> &packet);

    private:
        std::unique_ptr<VideoEncoderPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_VIDEOENCODER_HPP
