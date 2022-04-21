//
// Created by silas on 18.02.22.
//

#ifndef LIBAVQT_V4L2M2MDECODERIMPL_HPP
#define LIBAVQT_V4L2M2MDECODERIMPL_HPP

#include "AVQt/decoder/IVideoDecoderImpl.hpp"

#include <QObject>

namespace AVQt {
    class V4L2M2MDecoderImplPrivate;
    class V4L2M2MDecoderImpl : public QObject, public api::IVideoDecoderImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(V4L2M2MDecoderImpl)
        Q_INTERFACES(AVQt::api::IVideoDecoderImpl)
    public:
        static const api::VideoDecoderInfo &info();

        Q_INVOKABLE explicit V4L2M2MDecoderImpl(AVCodecID codec);
        ~V4L2M2MDecoderImpl() noexcept override;

        bool open(std::shared_ptr<AVCodecParameters> codecParams) override;
        void close() override;

        int decode(std::shared_ptr<AVPacket> packet) override;

        [[nodiscard]] bool isHWAccel() const override;
        [[nodiscard]] AVPixelFormat getOutputFormat() const override;
        [[nodiscard]] AVPixelFormat getSwOutputFormat() const override;
        [[nodiscard]] communication::VideoPadParams getVideoParams() const override;

    signals:
        void frameReady(std::shared_ptr<AVFrame> frame) override;

    protected:
        Q_INVOKABLE V4L2M2MDecoderImpl(AVCodecID codec, AVQt::V4L2M2MDecoderImplPrivate &dd, QObject *parent = nullptr);

        QScopedPointer<V4L2M2MDecoderImplPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_V4L2M2MDECODERIMPL_HPP
