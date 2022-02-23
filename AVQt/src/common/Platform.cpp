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
}// namespace AVQt::common
