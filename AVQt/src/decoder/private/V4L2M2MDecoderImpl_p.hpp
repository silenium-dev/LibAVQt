//
// Created by silas on 18.02.22.
//

#ifndef LIBAVQT_V4L2M2MDECODERIMPL_P_HPP
#define LIBAVQT_V4L2M2MDECODERIMPL_P_HPP

#include "global.hpp"

#include <QObject>

extern "C" {
#include <libavcodec/avcodec.h>
};

namespace AVQt {
    class V4L2M2MDecoderImpl;
    class V4L2M2MDecoderImplPrivate {
        Q_DECLARE_PUBLIC(V4L2M2MDecoderImpl)
    protected:
        class FrameFetcher : public QThread {
        public:
            explicit FrameFetcher(V4L2M2MDecoderImplPrivate *parent);
            void start();
            void stop();

        protected:
            void run() override;

        private:
            V4L2M2MDecoderImplPrivate *p;
            std::atomic_bool running{false};
        };

        explicit V4L2M2MDecoderImplPrivate(V4L2M2MDecoderImpl *q) : q_ptr(q) {}

        void init();

        static void destroyAVCodecContext(AVCodecContext *ctx);
        static void destroyAVCodecParameters(AVCodecParameters *par);
        static void destroyAVBufferRef(AVBufferRef *buf);

        static AVPixelFormat getFormat(AVCodecContext *ctx, const AVPixelFormat *fmt);

        V4L2M2MDecoderImpl *q_ptr;

        QMutex codecMutex{};

        AVCodecID codecId{AV_CODEC_ID_NONE};
        const AVCodec *codec{nullptr};
        std::shared_ptr<AVCodecContext> codecContext{nullptr, &destroyAVCodecContext};
        std::shared_ptr<AVCodecParameters> codecParameters{nullptr, &destroyAVCodecParameters};
        std::shared_ptr<AVBufferRef> hwDeviceContext{nullptr, &destroyAVBufferRef};
        std::shared_ptr<AVBufferRef> hwFramesContext{nullptr, &destroyAVBufferRef};

        std::unique_ptr<FrameFetcher> frameFetcher{};

        std::atomic_bool open{false};
        std::atomic_bool firstFrame{true};
        QWaitCondition firstFrameCond{};
    };
}// namespace AVQt


#endif//LIBAVQT_V4L2M2MDECODERIMPL_P_HPP
