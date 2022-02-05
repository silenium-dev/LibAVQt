//
// Created by silas on 27.01.22.
//

#include "AVQt/capture/DesktopCaptureFactory.hpp"
#include "AVQt/capture/IDesktopCaptureImpl.hpp"

#include "DummyDesktopCaptureImpl.hpp"
#ifndef Q_OS_ANDROID
#include "PipeWireDesktopCaptureImpl.hpp"
#endif

#include "global.hpp"

#include <QtCore>

namespace AVQt {
    DesktopCaptureFactory &DesktopCaptureFactory::getInstance() {
        static DesktopCaptureFactory instance;
        return instance;
    }

    bool DesktopCaptureFactory::registerCapture(const CaptureImplInfo &info) {
        QMutexLocker locker(&m_mutex);
        if (m_captureImpls.contains(info.name)) {
            return false;
        }
        m_captureImpls.insert(info.name, info);
        return true;
    }

    bool DesktopCaptureFactory::unregisterCapture(const QString &name) {
        QMutexLocker locker(&m_mutex);
        return m_captureImpls.remove(name);
    }

    bool DesktopCaptureFactory::isCaptureAvailable(common::Platform platform) {
        QMutexLocker locker(&m_mutex);
        return std::find_if(m_captureImpls.begin(), m_captureImpls.end(),
                            [platform](const CaptureImplInfo &info) {
                                return info.platform == platform;
                            }) != m_captureImpls.end();
    }

    std::shared_ptr<api::IDesktopCaptureImpl> DesktopCaptureFactory::createCapture(const QString &name) {
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
        for (auto &info : m_captureImpls) {
            if (info.platform == common::Platform::All) {
                impls.append(info);
            }
        }

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
        auto impl = qobject_cast<AVQt::api::IDesktopCaptureImpl *>(obj);
        return std::shared_ptr<api::IDesktopCaptureImpl>(impl);
    }

    void DesktopCaptureFactory::registerCaptures() {
        static std::atomic_bool registered{false};
        bool shouldBe = false;
        if (registered.compare_exchange_strong(shouldBe, true)) {
            // Register capture implementations
            getInstance().registerCapture({.metaObject = DummyDesktopCaptureImpl::staticMetaObject,
                                           .name = "AVQt::DummyDesktopCapture",
                                           .platform = common::Platform::All});
#if defined(Q_OS_LINUX) and !defined(Q_OS_ANDROID)
            getInstance().registerCapture({.metaObject = PipeWireDesktopCaptureImpl::staticMetaObject,
                                           .name = "AVQt::PipeWireDesktopCapture",
                                           .platform = common::Platform::Linux_Wayland});
#endif
        }
    }// namespace AVQt
}// namespace AVQt
