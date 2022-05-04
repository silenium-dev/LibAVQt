//
// Created by silas on 27.01.22.
//

#ifndef LIBAVQT_PLATFORM_HPP
#define LIBAVQT_PLATFORM_HPP

#include <QtCore/QList>

namespace AVQt::common {
    class Platform {
    public:
        enum Platform_t {
            Windows,
            Linux_Wayland,
            Linux_X11,
            MacOS,
            Android,
            iOS,
            All,
            Unknown
        };
        static Platform_t getPlatform();
        static bool isAvailable(const QList<Platform_t> &platforms);
        static std::string getName(const Platform_t &platform);
    };
}// namespace AVQt::common

#endif//LIBAVQT_PLATFORM_HPP
