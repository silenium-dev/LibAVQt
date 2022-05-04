//
// Created by silas on 28.01.22.
//

#ifndef LIBAVQT_DUMMYDESKTOPCAPTUREIMPL_HPP
#define LIBAVQT_DUMMYDESKTOPCAPTUREIMPL_HPP

#include "AVQt/capture/IDesktopCaptureImpl.hpp"

#include <QObject>
#include <QTimer>

namespace AVQt {
    class DummyDesktopCaptureImpl : public QObject, public api::IDesktopCaptureImpl {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IDesktopCaptureImpl)
    public:
        Q_INVOKABLE explicit DummyDesktopCaptureImpl(QObject *parent = nullptr);
        ~DummyDesktopCaptureImpl() override;

        bool open(const api::IDesktopCaptureImpl::Config &config) override;
        bool start() override;
        void close() override;

        [[nodiscard]] AVPixelFormat getOutputFormat() const override;
        [[nodiscard]] bool isHWAccel() const override;
        [[nodiscard]] communication::VideoPadParams getVideoParams() const override;

    private slots:
        void captureFrame();

    signals:
        void frameReady(std::shared_ptr<AVFrame> frame) override;

    private:
        std::unique_ptr<QTimer> m_timer{};
        std::atomic_int64_t m_frameCounter{0};
        int64_t m_ptsPerFrame{0};
    };
}// namespace AVQt


#endif//LIBAVQT_DUMMYDESKTOPCAPTUREIMPL_HPP
