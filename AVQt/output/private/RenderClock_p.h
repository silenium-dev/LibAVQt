//
// Created by silas on 3/31/21.
//

#include "../RenderClock.h"

#ifndef LIBAVQT_RENDERCLOCK_P_H
#define LIBAVQT_RENDERCLOCK_P_H

namespace AVQt {
    class RenderClockPrivate {
        explicit RenderClockPrivate(RenderClock *q) : q_ptr(q) {};
        RenderClock *q_ptr;

        QTimer *m_timer;
        std::atomic_int64_t m_interval = 1;
        std::atomic_bool m_active = false;

        friend class RenderClock;
    };
}

#endif //LIBAVQT_RENDERCLOCK_P_H
