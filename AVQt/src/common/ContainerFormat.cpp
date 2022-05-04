//
// Created by silas on 27/03/2022.
//

#include "common/ContainerFormat.hpp"

extern "C" {
#include <libavformat/avformat.h>
}

namespace AVQt::common {
    [[maybe_unused]] uint8_t ContainerFormat::getCapabilities(const char *formatName) {
        auto fmt = av_guess_format(formatName, nullptr, nullptr);
        if (fmt == nullptr) {
            return 0;
        }

        uint8_t capabilities = 0;

        if (fmt->video_codec != AV_CODEC_ID_NONE) {
            capabilities |= CONTAINER_FORMAT_CAP_VIDEO;
        }
        if (fmt->audio_codec != AV_CODEC_ID_NONE) {
            capabilities |= CONTAINER_FORMAT_CAP_AUDIO;
        }
        if (fmt->subtitle_codec != AV_CODEC_ID_NONE) {
            capabilities |= CONTAINER_FORMAT_CAP_SUBTITLE;
        }

        return capabilities;
    }

    [[maybe_unused]] uint8_t ContainerFormat::getCapabilities(const AVOutputFormat *fmt) {
        if (fmt == nullptr) {
            return 0;
        }

        uint8_t capabilities = 0;

        if (fmt->video_codec != AV_CODEC_ID_NONE) {
            capabilities |= CONTAINER_FORMAT_CAP_VIDEO;
        }
        if (fmt->audio_codec != AV_CODEC_ID_NONE) {
            capabilities |= CONTAINER_FORMAT_CAP_AUDIO;
        }
        if (fmt->subtitle_codec != AV_CODEC_ID_NONE) {
            capabilities |= CONTAINER_FORMAT_CAP_SUBTITLE;
        }

        return capabilities;
    }
}// namespace AVQt::common
