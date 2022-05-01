//
// Created by silas on 22.02.22.
//
#include "common/Platform.hpp"

#include <QList>
#include <QProcessEnvironment>
#include <QtGlobal>

namespace AVQt::common {
    Platform::Platform_t Platform::getPlatform() {
#if defined(Q_OS_WINDOWS)
        return Platform::Windows;
#elif defined(Q_OS_MACOS)
        return Platform::MacOS;
#elif defined(Q_OS_LINUX) and not defined(Q_OS_ANDROID)
        bool isWayland = QProcessEnvironment::systemEnvironment().value("XDG_SESSION_TYPE", "").toLower() == "wayland";
        return isWayland ? Platform::Linux_Wayland : Platform::Linux_X11;
#elif defined(Q_OS_ANDROID)
        return Platform::Android;
#elif defined(Q_OS_IOS)
        return Platform::iOS;
#else
        return Platform::Unknown;
#endif
    }

    bool Platform::isAvailable(const QList<Platform_t> &platforms) {
        return platforms.contains(All) || platforms.contains(getPlatform());
    }

    std::string Platform::getName(const Platform::Platform_t &platform) {
        switch (platform) {
            case Platform::Windows:
                return "Windows";
            case Platform::MacOS:
                return "MacOS";
            case Platform::Linux_X11:
                return "Linux (X11)";
            case Platform::Linux_Wayland:
                return "Linux (Wayland)";
            case Platform::Android:
                return "Android";
            case Platform::iOS:
                return "iOS";
            default:
                return "Unknown";
        }
    }
}// namespace AVQt::common
