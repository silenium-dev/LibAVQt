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

#include "AVQt/communication/IComponent.hpp"
#include "AVQt/decoder/IVideoDecoderImpl.hpp"

#include <QtCore/QThread>
#include <pgraph/impl/SimpleProcessor.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}


#ifndef TRANSCODE_DECODERVAAPI_H
#define TRANSCODE_DECODERVAAPI_H

namespace AVQt {
    class VideoDecoderPrivate;

    class VideoDecoder : public QThread, public api::IComponent, public pgraph::impl::SimpleProcessor {
        Q_OBJECT
        Q_DECLARE_PRIVATE(AVQt::VideoDecoder)
        Q_INTERFACES(AVQt::api::IComponent)

    public:
        struct Config {
            QStringList decoderPriority{};
        };

        explicit VideoDecoder(const Config &config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        explicit VideoDecoder(const Config &config, QObject *parent = nullptr);

        /*!
         * \private
         */
        VideoDecoder(const VideoDecoder &) = delete;

        /*!
         * \private
         */
        void operator=(const VideoDecoder &) = delete;
        bool isOpen() const override;
        bool isRunning() const override;
        /*!
         * \private
         */
        ~VideoDecoder() Q_DECL_OVERRIDE;

        /*!
         * \brief Returns paused state of decoder
         * @return Paused state
         */
        bool isPaused() const Q_DECL_OVERRIDE;

        [[nodiscard]] int64_t getInputPadId() const;

        [[nodiscard]] int64_t getOutputPadId() const;

        bool init() Q_DECL_OVERRIDE;

    protected slots:
        bool open() Q_DECL_OVERRIDE;

        void close() Q_DECL_OVERRIDE;

        bool start() Q_DECL_OVERRIDE;

        void stop() Q_DECL_OVERRIDE;

        void pause(bool pause) Q_DECL_OVERRIDE;

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

    protected slots:
        void onFrameReady(const std::shared_ptr<AVFrame> &frame);

    protected:
        /*!
         * \private
         */
        void run() Q_DECL_OVERRIDE;

        /*!
         * \private
         */
        VideoDecoderPrivate *d_ptr;
    };

}// namespace AVQt

#endif//TRANSCODE_DECODERVAAPI_H
