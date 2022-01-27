//
// Created by silas on 27.01.22.
//

#include "capture/VideoCapturer.hpp"
#include "capture/VideoCaptureFactory.hpp"
#include "private/DesktopCapturer_p.hpp"

#include <pgraph_network/impl/RegisteringPadFactory.hpp>

namespace AVQt {
    VideoCapturer::VideoCapturer(const VideoCapturer::Settings &settings, std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProducer(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new VideoCapturerPrivate(this)) {
        Q_D(VideoCapturer);
        d->impl = VideoCaptureFactory::getInstance().createCapture();
    }

    VideoCapturer::~VideoCapturer() {
    }
}// namespace AVQt
