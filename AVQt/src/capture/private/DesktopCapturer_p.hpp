//
// Created by silas on 27.01.22.
//

#ifndef LIBAVQT_DESKTOPCAPTURER_P_HPP
#define LIBAVQT_DESKTOPCAPTURER_P_HPP

#include "capture/IVideoCaptureImpl.hpp"
#include "capture/VideoCapturer.hpp"

#include <QObject>

extern "C" {
#include <libavcodec/avcodec.h>
};

namespace AVQt {
    class VideoCapturer;
    class VideoCapturerPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(VideoCapturer)
    protected slots:
        void onFrameCaptured(const std::shared_ptr<AVFrame> &frame);

    private:
        explicit VideoCapturerPrivate(VideoCapturer *q) : q_ptr(q) {}

        VideoCapturer *q_ptr;

        std::shared_ptr<api::IVideoCaptureImpl> impl{};
    };
}// namespace AVQt


#endif//LIBAVQT_DESKTOPCAPTURER_P_HPP
