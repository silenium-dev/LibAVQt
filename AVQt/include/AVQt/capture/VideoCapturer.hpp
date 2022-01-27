//
// Created by silas on 27.01.22.
//

#ifndef LIBAVQT_VIDEOCAPTURER_HPP
#define LIBAVQT_VIDEOCAPTURER_HPP

#include "communication/IComponent.hpp"
#include <QThread>
#include <pgraph/impl/SimpleProducer.hpp>
#include <pgraph_network/api/PadRegistry.hpp>

namespace AVQt {
    class VideoCapturerPrivate;
    class VideoCapturer : public QThread, public api::IComponent, public pgraph::impl::SimpleProducer {
        Q_OBJECT
        Q_DECLARE_PRIVATE(VideoCapturer)
        Q_DISABLE_COPY(VideoCapturer)
    public:
        enum class CaptureMode {
            Screen,
            Window,
            Any
        };
        struct Settings {
            int fps{30};
            CaptureMode mode{CaptureMode::Any};
        };

        explicit VideoCapturer(const Settings &settings, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        ~VideoCapturer() override;

    private:
        QScopedPointer<VideoCapturerPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_VIDEOCAPTURER_HPP
