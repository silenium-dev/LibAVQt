#ifndef LIBAVQT_MEDIACODECDECODERIMPLPRIVATE_HPP
#define LIBAVQT_MEDIACODECDECODERIMPLPRIVATE_HPP

#include <QMutex>
#include <QObject>
#include <QThread>
#include <QWaitCondition>

#include <memory>


extern "C" {
#include <libavcodec/avcodec.h>
}

namespace AVQt {
    class MediaCodecDecoderImpl;
    class MediaCodecDecoderImplPrivate {
        Q_DECLARE_PUBLIC(MediaCodecDecoderImpl)
    protected:
        static void destroyAVCodecContext(AVCodecContext *ctx);
        static void destroyAVCodecParameters(AVCodecParameters *par);
        static void destroyAVBufferRef(AVBufferRef *buf);
        static AVPixelFormat getFormat(AVCodecContext *ctx, const AVPixelFormat *fmt);

    private:
        class FrameFetcher : public QThread {
        public:
            explicit FrameFetcher(MediaCodecDecoderImplPrivate *parent);
            void start();
            void stop();

        protected:
            void run() override;

        private:
            MediaCodecDecoderImplPrivate *p;
            std::atomic_bool running{false};
        };

        explicit MediaCodecDecoderImplPrivate(MediaCodecDecoderImpl *q) : q_ptr(q) {}
        MediaCodecDecoderImpl *q_ptr;

        void init();

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

#endif//LIBAVQT_MEDIACODECDECODERIMPLPRIVATE_HPP
