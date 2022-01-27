//
// Created by silas on 27.01.22.
//

#include "capture/VideoCaptureFactory.hpp"

#include <QtCore>

namespace AVQt {
    VideoCaptureFactory &VideoCaptureFactory::getInstance() {
        static VideoCaptureFactory instance;
        return instance;
    }

    bool VideoCaptureFactory::registerCapture(const CaptureImplInfo &info) {
        QMutexLocker locker(&m_mutex);
        if (m_captureImpls.contains(info.name)) {
            return false;
        }
        m_captureImpls.insert(info.name, info);
        return true;
    }

    bool VideoCaptureFactory::unregisterCapture(const QString &name) {
        QMutexLocker locker(&m_mutex);
        return m_captureImpls.remove(name);
    }

    bool VideoCaptureFactory::isCaptureAvailable(common::Platform platform) {
        QMutexLocker locker(&m_mutex);
        return std::find_if(m_captureImpls.begin(), m_captureImpls.end(),
                            [platform](const CaptureImplInfo &info) {
                                return info.platform == platform;
                            }) != m_captureImpls.end();
    }

    std::shared_ptr<api::IVideoCaptureImpl> VideoCaptureFactory::createCapture(const QString &name) {
        QMutexLocker locker(&m_mutex);
        if (m_captureImpls.isEmpty()) {
            return {};
        }
        QList<CaptureImplInfo> impls;
#ifdef Q_OS_WIN
        for (auto &info : m_captureImpls) {
            if (info.platform == common::Platform::Windows) {
                impls.append(info);
            }
        }
#elif defined(Q_OS_ANDROID)
        for (auto &info : m_captureImpls) {
            if (info.platform == common::Platform::Android) {
                impls.append(info);
            }
        }
#elif defined(Q_OS_IOS)
        for (auto &info : m_captureImpls) {
            if (info.platform == common::Platform::iOS) {
                impls.append(info);
            }
        }
#elif defined(Q_OS_LINUX)
        bool isWayland = QProcessEnvironment::systemEnvironment().value("XDG_SESSION_TYPE") == "wayland";
        for (auto &info : m_captureImpls) {
            if (info.platform == (isWayland ? common::Platform::Linux_Wayland : common::Platform::Linux_X11)) {
                impls.append(info);
            }
        }
#elif defined(Q_OS_MACOS)
        for (auto &info : m_captureImpls) {
            if (info.platform == common::Platform::MacOS) {
                impls.append(info);
            }
        }
#endif

        if (impls.isEmpty()) {
            return {};
        }

        QMetaObject metaObject{};
        if (!name.isEmpty()) {
            auto it = std::find_if(impls.begin(), impls.end(),
                                   [name](const CaptureImplInfo &info) {
                                       return info.name == name;
                                   });
            if (it != impls.end()) {
                metaObject = it->metaObject;
            } else {
                return {};
            }
        } else {
            metaObject = impls.first().metaObject;
        }

        QObject *obj = metaObject.newInstance();
        return std::shared_ptr<api::IVideoCaptureImpl>(qobject_cast<api::IVideoCaptureImpl *>(obj));
    }
}// namespace AVQt
