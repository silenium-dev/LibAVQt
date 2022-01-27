//
// Created by silas on 27.01.22.
//

#ifndef LIBAVQT_VIDEOCAPTUREFACTORY_HPP
#define LIBAVQT_VIDEOCAPTUREFACTORY_HPP

#include "capture/IVideoCaptureImpl.hpp"
#include "common/Platform.hpp"

#include <QMap>
#include <QMutex>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace AVQt {
    class VideoCaptureFactory {
    public:
        struct CaptureImplInfo {
            QMetaObject metaObject;
            QString name;
            common::Platform platform;
        };
        static VideoCaptureFactory &getInstance() Q_DECL_UNUSED;

        bool isCaptureAvailable(common::Platform platform) Q_DECL_UNUSED;
        bool registerCapture(const CaptureImplInfo &info) Q_DECL_UNUSED;
        bool unregisterCapture(const QString &name) Q_DECL_UNUSED;

        std::shared_ptr<api::IVideoCaptureImpl> createCapture(const QString &name = "") Q_DECL_UNUSED;

    private:
        VideoCaptureFactory() = default;

        QMutex m_mutex;
        QMap<QString, CaptureImplInfo> m_captureImpls;
    };
}// namespace AVQt


#endif//LIBAVQT_VIDEOCAPTUREFACTORY_HPP
