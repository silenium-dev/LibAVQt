#include "renderers/RenderClock.h"

#ifndef LIBAVQT_RENDERCLOCK_P_H
#define LIBAVQT_RENDERCLOCK_P_H

namespace AVQt {
    class RenderClockPrivate : public QObject {
    Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::RenderClock)

    public:
        RenderClockPrivate(const RenderClockPrivate &) = delete;

        void operator=(const RenderClockPrivate &) = delete;

    private:
        explicit RenderClockPrivate(RenderClock *q) : q_ptr(q) {};
        RenderClock *q_ptr;

        QTimer *m_timer{nullptr};
        std::atomic_int m_interval{1};
        std::atomic_bool m_active{false};
        std::atomic_bool m_paused{false};
        QElapsedTimer *m_elapsedTime{nullptr};
        qint64 m_lastPauseTimestamp{0};

        friend class RenderClock;
    };
}

#endif //LIBAVQT_RENDERCLOCK_P_H
