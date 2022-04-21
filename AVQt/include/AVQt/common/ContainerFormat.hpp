//
// Created by silas on 27/03/2022.
//

#ifndef LIBAVQT_CONTAINERFORMAT_HPP
#define LIBAVQT_CONTAINERFORMAT_HPP

#include <QtCore/QString>

extern "C" {
#include <libavformat/avformat.h>
}

namespace AVQt::common {
    constexpr int CONTAINER_FORMAT_CAP_VIDEO = 0b00000001;
    constexpr int CONTAINER_FORMAT_CAP_AUDIO = 0b00000010;
    constexpr int CONTAINER_FORMAT_CAP_SUBTITLE = 0b00000100;

    class ContainerFormat {
    public:
        [[maybe_unused]] static constexpr const char *MPEGTS = "mpegts";
        [[maybe_unused]] static constexpr const char *MP4 = "mp4";
        [[maybe_unused]] static constexpr const char *MKV = "matroska";
        [[maybe_unused]] static constexpr const char *WEBM = "webm";
        [[maybe_unused]] static constexpr const char *MP3 = "mp3";
        [[maybe_unused]] static constexpr const char *M4A = "m4a";

        [[maybe_unused]] static uint8_t getCapabilities(const char *formatName);
        [[maybe_unused]] static uint8_t getCapabilities(const AVOutputFormat *fmt);
    };
}// namespace AVQt::common


#endif//LIBAVQT_CONTAINERFORMAT_HPP
