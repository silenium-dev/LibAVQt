//
// Created by silas on 28.01.22.
//

#include "PipeWireDesktopCaptureImpl.hpp"
#include "private/PipeWireDesktopCaptureImpl_p.hpp"

#include <libdrm/drm_fourcc.h>

extern "C" {
#include <libavutil/hwcontext_drm.h>
}

namespace AVQt {
    PipeWireDesktopCaptureImpl::PipeWireDesktopCaptureImpl(QObject *parent)
        : QObject(parent), d_ptr(new PipeWireDesktopCaptureImplPrivate(this)) {
    }

    PipeWireDesktopCaptureImpl::~PipeWireDesktopCaptureImpl() = default;

    bool PipeWireDesktopCaptureImpl::open(const api::IDesktopCaptureImpl::Config &config) {
        Q_D(PipeWireDesktopCaptureImpl);
        d->config = config;
        ScreenCapture::PipeWireCapturer::CaptureSourceType type;
        switch (config.sourceClass) {
            case api::IDesktopCaptureImpl::SourceClass::Screen:
                type = ScreenCapture::PipeWireCapturer::CaptureSourceType::kScreen;
                break;
            case api::IDesktopCaptureImpl::SourceClass::Window:
                type = ScreenCapture::PipeWireCapturer::CaptureSourceType::kWindow;
                break;
            case api::IDesktopCaptureImpl::SourceClass::Any:
                type = ScreenCapture::PipeWireCapturer::CaptureSourceType::kAny;
                break;
        }
        d->capturer = std::make_unique<ScreenCapture::PipeWireCapturer>(type);
        return static_cast<bool>(d->capturer);
    }

    bool PipeWireDesktopCaptureImpl::start() {
        Q_D(PipeWireDesktopCaptureImpl);

        d->capturer->Start(d, 1000 / d->config.fps);

        return true;
    }

    void PipeWireDesktopCaptureImpl::close() {
        Q_D(PipeWireDesktopCaptureImpl);
        d->capturer->Stop();
        d->capturer.reset();
    }

    AVPixelFormat PipeWireDesktopCaptureImpl::getOutputFormat() const {
        return AV_PIX_FMT_DRM_PRIME;
    }

    AVPixelFormat PipeWireDesktopCaptureImpl::getSwOutputFormat() const {
        Q_D(const PipeWireDesktopCaptureImpl);

        switch (d->lastDrmFormat) {
            case DRM_FORMAT_ARGB8888:
                return AV_PIX_FMT_BGRA;
            case DRM_FORMAT_XRGB8888:
                return AV_PIX_FMT_BGR0;
            case DRM_FORMAT_ABGR8888:
                return AV_PIX_FMT_RGBA;
            case DRM_FORMAT_XBGR8888:
                return AV_PIX_FMT_RGB0;
            default:
                return AV_PIX_FMT_NONE;
        }
    }

    bool PipeWireDesktopCaptureImpl::isHWAccel() const {
        return true;
    }

    communication::VideoPadParams PipeWireDesktopCaptureImpl::getVideoParams() const {
        Q_D(const PipeWireDesktopCaptureImpl);

        communication::VideoPadParams params;
        params.pixelFormat = getOutputFormat();
        params.swPixelFormat = getSwOutputFormat();
        params.isHWAccel = isHWAccel();
        params.frameSize = d->lastFrameSize;
        params.hwFramesContext = nullptr;
        params.hwDeviceContext = nullptr;

        return params;
    }

    void PipeWireDesktopCaptureImplPrivate::OnCaptureResult(ScreenCapture::PipeWireCapturer::Result result, std::unique_ptr<ScreenCapture::DesktopFrame> ptr) {
        Q_Q(PipeWireDesktopCaptureImpl);

        if (result == ScreenCapture::PipeWireCapturer::Result::SUCCESS) {
            std::shared_ptr<ScreenCapture::DmaBufDesktopFrame> dmaBufFrame{dynamic_cast<ScreenCapture::DmaBufDesktopFrame *>(ptr.release())};
            emit q->frameReady(convertToAVFrame(dmaBufFrame));
        }
    }

    std::shared_ptr<AVFrame> PipeWireDesktopCaptureImplPrivate::convertToAVFrame(const std::shared_ptr<ScreenCapture::DmaBufDesktopFrame> &frame) {
        if (!frame) {
            return nullptr;
        }

        std::shared_ptr<AVFrame> avFrame{av_frame_alloc(), [](AVFrame *frame) {
                                             av_frame_free(&frame);
                                         }};
        if (!avFrame) {
            return nullptr;
        }

        avFrame->format = AV_PIX_FMT_DRM_PRIME;
        avFrame->width = frame->size().width();
        avFrame->height = frame->size().height();
        avFrame->pts = frame->capture_time_ms() * 1000;

        auto dmaBufs = *reinterpret_cast<std::vector<std::shared_ptr<ScreenCapture::DmaBufDesktopFrame::DmaBuf>> *>(frame->data());

        lastDrmFormat = frame->drm_format();
        lastFrameSize = frame->size();

        auto *buf = av_buffer_alloc(sizeof(AVDRMFrameDescriptor));
        if (!buf) {
            return nullptr;
        }

        auto *desc = reinterpret_cast<AVDRMFrameDescriptor *>(buf->data);

        desc->nb_layers = 1;
        desc->nb_objects = 1;
        desc->layers[0].nb_planes = 1;
        desc->layers[0].format = frame->drm_format();
        desc->layers[0].planes[0].object_index = 0;
        desc->layers[0].planes[0].offset = dmaBufs.at(0)->offset;
        desc->layers[0].planes[0].pitch = dmaBufs.at(0)->stride;
        desc->objects[0].fd = dmaBufs.at(0)->fd;
        desc->objects[0].size = dmaBufs.at(0)->stride * frame->size().height();
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_INVALID;

        avFrame->data[0] = reinterpret_cast<uint8_t *>(desc);
        avFrame->buf[0] = buf;

        return avFrame;
    }
}// namespace AVQt
