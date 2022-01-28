//
// Created by silas on 28.01.22.
//

#ifndef LIBAVQT_PIPEWIREDESKTOPCAPTUREIMPL_P_HPP
#define LIBAVQT_PIPEWIREDESKTOPCAPTUREIMPL_P_HPP

#include <QObject>
#include <ScreenCapture/pipewire/PipeWireCapturer.hpp>

namespace AVQt {
    class PipeWireDesktopCaptureImpl;
    class PipeWireDesktopCaptureImplPrivate : public ScreenCapture::PipeWireCapturer::Callback {
        Q_DECLARE_PUBLIC(PipeWireDesktopCaptureImpl)
    public:
        ~PipeWireDesktopCaptureImplPrivate() override = default;

        void OnCaptureResult(ScreenCapture::PipeWireCapturer::Result result, std::unique_ptr<ScreenCapture::DesktopFrame> ptr) override;

    private:
        explicit PipeWireDesktopCaptureImplPrivate(PipeWireDesktopCaptureImpl *q) : q_ptr(q) {}

        std::shared_ptr<AVFrame> convertToAVFrame(const std::shared_ptr<ScreenCapture::DmaBufDesktopFrame> &frame);

        PipeWireDesktopCaptureImpl *q_ptr;

        std::unique_ptr<ScreenCapture::PipeWireCapturer> capturer{};

        api::IDesktopCaptureImpl::Config config{};

        uint32_t lastDrmFormat{DRM_FORMAT_INVALID};
        QSize lastFrameSize{};
    };
}// namespace AVQt


#endif//LIBAVQT_PIPEWIREDESKTOPCAPTUREIMPL_P_HPP
