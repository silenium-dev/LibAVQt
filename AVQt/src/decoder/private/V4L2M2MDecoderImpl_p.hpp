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
        explicit V4L2M2MDecoderImplPrivate(V4L2M2MDecoderImpl *q) : q_ptr(q) {}

        void init();

        static void destroyAVCodecContext(AVCodecContext *ctx);
        static void destroyAVCodecParameters(AVCodecParameters *par);
        static void destroyAVBufferRef(AVBufferRef *buf);

        V4L2M2MDecoderImpl *q_ptr;

        AVCodecID codecId{AV_CODEC_ID_NONE};
        AVCodec *codec{nullptr};
        std::shared_ptr<AVCodecContext> codecContext{nullptr, &destroyAVCodecContext};
        std::shared_ptr<AVCodecParameters> codecParameters{nullptr, &destroyAVCodecParameters};
        std::shared_ptr<AVBufferRef> hwDeviceContext{nullptr, &destroyAVBufferRef};
        std::shared_ptr<AVBufferRef> hwFramesContext{nullptr, &destroyAVBufferRef};
    };
}// namespace AVQt


#endif//LIBAVQT_V4L2M2MDECODERIMPL_P_HPP
