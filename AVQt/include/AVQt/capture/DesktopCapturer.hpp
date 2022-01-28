//
// Created by silas on 27.01.22.
//

#ifndef LIBAVQT_DESKTOPCAPTURER_HPP
#define LIBAVQT_DESKTOPCAPTURER_HPP

#include "IDesktopCaptureImpl.hpp"
#include "AVQt/communication/IComponent.hpp"
#include <QThread>
#include <pgraph/impl/SimpleProducer.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

namespace AVQt {
    class DesktopCapturerPrivate;
    class DesktopCapturer : public QThread, public api::IComponent, public pgraph::impl::SimpleProducer {
        Q_OBJECT
        Q_INTERFACES(AVQt::api::IComponent)
        Q_DECLARE_PRIVATE(DesktopCapturer)
    public:
        explicit DesktopCapturer(const api::IDesktopCaptureImpl::Config &config, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        ~DesktopCapturer() override;

        bool isOpen() const override;
        bool isRunning() const override;
        bool isPaused() const override;

    public slots:
        bool init() override;

        bool open() override;
        void close() override;

        bool start() override;
        void stop() override;

        void pause(bool state) override;

    signals:
        void started() override;
        void stopped() override;
        void paused(bool state) override;

    protected:
        void run() override;

    private:
        QScopedPointer<DesktopCapturerPrivate> d_ptr;

        Q_DISABLE_COPY(DesktopCapturer)
    };
}// namespace AVQt


#endif//LIBAVQT_DESKTOPCAPTURER_HPP
