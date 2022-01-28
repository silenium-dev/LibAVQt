//
// Created by silas on 28.01.22.
//

#include "DummyDesktopCaptureImpl.hpp"

#include <QtDebug>

extern "C" {
#include <libavutil/imgutils.h>
}

namespace AVQt {
    DummyDesktopCaptureImpl::DummyDesktopCaptureImpl(QObject *parent) : QObject(parent) {
    }

    DummyDesktopCaptureImpl::~DummyDesktopCaptureImpl() {
        DummyDesktopCaptureImpl::close();
    }

    bool DummyDesktopCaptureImpl::open(const api::IDesktopCaptureImpl::Config &config) {
        m_ptsPerFrame = 1000000 / config.fps;
        m_timer = std::make_unique<QTimer>();
        m_timer->setInterval(1000 / config.fps);
        m_timer->setSingleShot(false);
        return true;
    }

    bool DummyDesktopCaptureImpl::start() {
        connect(m_timer.get(), &QTimer::timeout, this, &DummyDesktopCaptureImpl::captureFrame, Qt::QueuedConnection);
        m_timer->start();
        return true;
    }

    void DummyDesktopCaptureImpl::close() {
        if (m_timer && m_timer->isActive()) {
            m_timer->stop();
            m_timer.reset();
        }
    }

    void DummyDesktopCaptureImpl::captureFrame() {
        std::shared_ptr<AVFrame> frame{av_frame_alloc(), [](AVFrame *frame) {
                                           av_frame_free(&frame);
                                       }};
        frame->format = AV_PIX_FMT_RGB24;
        frame->width = 1920;
        frame->height = 1080;
        frame->pts = m_frameCounter++ * m_ptsPerFrame;
        int ret = av_image_alloc(frame->data, frame->linesize, frame->width, frame->height, AV_PIX_FMT_RGB24, 1);
        if (ret < 0) {
            char errbuf[128];
            qDebug() << "av_image_alloc failed" << av_make_error_string(errbuf, sizeof(errbuf), ret);
            return;
        }

        emit frameReady(std::move(frame));
    }

    AVPixelFormat DummyDesktopCaptureImpl::getOutputFormat() const {
        return AV_PIX_FMT_RGB24;
    }

    bool DummyDesktopCaptureImpl::isHWAccel() const {
        return false;
    }
    communication::VideoPadParams DummyDesktopCaptureImpl::getVideoParams() const {
        communication::VideoPadParams params{};
        params.hwDeviceContext = nullptr,
        params.hwFramesContext = nullptr,
        params.isHWAccel = false,
        params.swPixelFormat = AV_PIX_FMT_RGB24,
        params.pixelFormat = AV_PIX_FMT_RGB24,
        params.frameSize = {1920, 1080};
        return params;
    }

}// namespace AVQt
