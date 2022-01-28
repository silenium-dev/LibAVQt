//
// Created by silas on 27.01.22.
//

#ifndef LIBAVQT_DESKTOPCAPTURER_P_HPP
#define LIBAVQT_DESKTOPCAPTURER_P_HPP

#include "capture/IDesktopCaptureImpl.hpp"

#include <QObject>
#include <pgraph/api/Pad.hpp>
#include <pgraph/api/PadUserData.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
};

namespace AVQt {
    class DesktopCapturer;
    class DesktopCapturerPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(DesktopCapturer)
    protected slots:
        void onFrameCaptured(const std::shared_ptr<AVFrame> &frame);

    private:
        explicit DesktopCapturerPrivate(DesktopCapturer *q) : q_ptr(q) {}

        DesktopCapturer *q_ptr;

        api::IDesktopCaptureImpl::Config config{};
        std::shared_ptr<api::IDesktopCaptureImpl> impl{};

        QSize lastFrameSize{};

        QMetaObject::Connection frameReadyConnection{};
        QThread *afterStopThread{};

        std::atomic_bool initialized{false}, open{false}, running{false}, paused{false};

        int64_t outputPadId{pgraph::api::INVALID_PAD_ID};
        std::shared_ptr<communication::VideoPadParams> outputPadUserData{};
    };
}// namespace AVQt


#endif//LIBAVQT_DESKTOPCAPTURER_P_HPP
