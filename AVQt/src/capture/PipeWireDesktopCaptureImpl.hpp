//
// Created by silas on 28.01.22.
//

#ifndef LIBAVQT_PIPEWIREDESKTOPCAPTUREIMPL_HPP
#define LIBAVQT_PIPEWIREDESKTOPCAPTUREIMPL_HPP

#include "ScreenCapture/pipewire/PipeWireCapturer.hpp"
#include "AVQt/capture/IDesktopCaptureImpl.hpp"

#include <QObject>

namespace AVQt {
    class PipeWireDesktopCaptureImplPrivate;
    class PipeWireDesktopCaptureImpl : public QObject, public api::IDesktopCaptureImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(PipeWireDesktopCaptureImpl)
        Q_INTERFACES(AVQt::api::IDesktopCaptureImpl)
    public:
        Q_INVOKABLE explicit PipeWireDesktopCaptureImpl(QObject *parent = nullptr);
        ~PipeWireDesktopCaptureImpl() override;

        bool open(const api::IDesktopCaptureImpl::Config &config) override;
        bool start() override;
        void close() override;

        [[nodiscard]] AVPixelFormat getOutputFormat() const override;
        [[nodiscard]] AVPixelFormat getSwOutputFormat() const override;

        [[nodiscard]] bool isHWAccel() const override;
        [[nodiscard]] communication::VideoPadParams getVideoParams() const override;

    signals:
        void frameReady(std::shared_ptr<AVFrame> frame) override;

    private:
        QScopedPointer<PipeWireDesktopCaptureImplPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_PIPEWIREDESKTOPCAPTUREIMPL_HPP
