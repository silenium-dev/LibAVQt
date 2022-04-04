//
// Created by silas on 03/04/2022.
//

#ifndef LIBAVQT_QSVDECODERIMPL_HPP
#define LIBAVQT_QSVDECODERIMPL_HPP

#include "decoder/IVideoDecoderImpl.hpp"

#include <QObject>

namespace AVQt {
    class QSVDecoderImplPrivate;
    class QSVDecoderImpl : public QObject, public api::IVideoDecoderImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(QSVDecoderImpl)
    public:
        static const api::VideoDecoderInfo &info();

        explicit QSVDecoderImpl(AVCodecID codecId, QObject *parent = nullptr);
        ~QSVDecoderImpl() override;
        bool open(std::shared_ptr<AVCodecParameters> codecParams) override;
        void close() override;
        int decode(std::shared_ptr<AVPacket> packet) override;
        [[nodiscard]] AVPixelFormat getOutputFormat() const override;
        [[nodiscard]] AVPixelFormat getSwOutputFormat() const override;
        [[nodiscard]] bool isHWAccel() const override;
        [[nodiscard]] communication::VideoPadParams getVideoParams() const override;

    signals:
        void frameReady(std::shared_ptr<AVFrame> frame) override;

    protected:
        QScopedPointer<QSVDecoderImplPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_QSVDECODERIMPL_HPP
