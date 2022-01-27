//
// Created by silas on 27.01.22.
//

#ifndef LIBAVQT_PLATFORM_HPP
#define LIBAVQT_PLATFORM_HPP

namespace AVQt::common {
    enum class Platform {
        Windows,
        Linux_Wayland,
        Linux_X11,
        MacOS,
        Android,
        iOS,
        Unknown
    };
}

#endif//LIBAVQT_PLATFORM_HPP
