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

        QTimer *m_timer {nullptr};
        std::atomic_int64_t m_interval = 1;
        std::atomic_bool m_active  {false};
        std::atomic_bool m_paused  {false};
        QElapsedTimer *m_elapsedTime {nullptr};
        qint64 m_pausedTime {0};
        qint64 m_lastPauseTimestamp {0};

        friend class RenderClock;
    };
}

#endif //LIBAVQT_RENDERCLOCK_P_H