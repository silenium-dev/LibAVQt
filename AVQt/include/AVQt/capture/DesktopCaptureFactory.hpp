//
// Created by silas on 27.01.22.
//

#ifndef LIBAVQT_DESKTOPCAPTUREFACTORY_HPP
#define LIBAVQT_DESKTOPCAPTUREFACTORY_HPP

#include "AVQt/capture/IDesktopCaptureImpl.hpp"
#include "AVQt/common/Platform.hpp"

#include <QMap>
#include <QMutex>

#include <static_block.hpp>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace AVQt {
    class DesktopCaptureFactory {
    public:
        struct CaptureImplInfo {
            QMetaObject metaObject;
            QString name;
            common::Platform platform;
        };
        static DesktopCaptureFactory &getInstance() Q_DECL_UNUSED;

        bool isCaptureAvailable(common::Platform platform) Q_DECL_UNUSED;
        bool registerCapture(const CaptureImplInfo &info) Q_DECL_UNUSED;
        bool unregisterCapture(const QString &name) Q_DECL_UNUSED;

        /**
         * @brief Creates a new instance of a capture implementation for the current platform.
         * @param name The name of the capture implementation (optional, ignored if empty).
         * @return A new instance of the capture implementation.
         */
        std::shared_ptr<api::IDesktopCaptureImpl> createCapture(const QString &name = "") Q_DECL_UNUSED;

        static void registerCaptures();

    private:
        DesktopCaptureFactory() = default;

        QMutex m_mutex;
        QMap<QString, CaptureImplInfo> m_captureImpls;
    };
}// namespace AVQt

static_block {
    AVQt::DesktopCaptureFactory::registerCaptures();
};

#endif//LIBAVQT_DESKTOPCAPTUREFACTORY_HPP
