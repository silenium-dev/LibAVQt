//
// Created by silas on 03/04/2022.
//

#ifndef LIBAVQT_QSVDECODERIMPL_P_HPP
#define LIBAVQT_QSVDECODERIMPL_P_HPP

#include <QObject>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
};

namespace AVQt {
    class QSVDecoderImpl;
    class QSVDecoderImplPrivate {
        Q_DECLARE_PUBLIC(QSVDecoderImpl)
    public:
        static void destroyAVCodecContext(AVCodecContext *codecContext);
        static void destroyAVBufferRef(AVBufferRef *buffer);

        static AVPixelFormat getFormat(AVCodecContext *codecContext, const AVPixelFormat *pixFmts);

    private:
        explicit QSVDecoderImplPrivate(QSVDecoderImpl *q) : q_ptr(q) {}
        QSVDecoderImpl *q_ptr;

        class FrameFetcher : public QThread {
        public:
            explicit FrameFetcher(QSVDecoderImplPrivate *d);
            ~FrameFetcher() override;

            void run() override;
            void stop();

        private:
            QSVDecoderImplPrivate *p;
            std::atomic_bool m_stop{false};
        };

        AVCodecID codecId{};
        const AVCodec *pCodec{};
        std::shared_ptr<AVCodecParameters> codecParams{};

        QMutex codecMutex{};
        std::shared_ptr<AVCodecContext> codecContext{nullptr, &destroyAVCodecContext};

        std::shared_ptr<AVBufferRef> hwDeviceContext{nullptr, &destroyAVBufferRef};
        std::shared_ptr<AVBufferRef> hwFramesContext{nullptr, &destroyAVBufferRef};

        std::atomic_bool open{false}, firstFrame{true};

        std::unique_ptr<FrameFetcher> frameFetcher{};
    };
}// namespace AVQt


#endif//LIBAVQT_QSVDECODERIMPL_P_HPP
