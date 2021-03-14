//
// Created by silas on 3/14/21.
//

#include "../FrameSaver.h"

#include <QObject>

#ifndef TRANSCODE_FRAMESAVER_P_H
#define TRANSCODE_FRAMESAVER_P_H

namespace AV {
    struct FrameSaverPrivate {
        explicit FrameSaverPrivate(FrameSaver *q) : q_ptr(q) {}

        FrameSaver *q_ptr;

        std::atomic_uint64_t m_frameNumber = 0;
        std::atomic_bool m_isPaused = false;
    };
}

#endif //TRANSCODE_FRAMESAVER_P_H
