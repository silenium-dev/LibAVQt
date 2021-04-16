//
// Created by silas on 3/31/21.
//

#include "private/RenderClock_p.h"
#include "RenderClock.h"

namespace AVQt {
    RenderClock::RenderClock(QObject *parent) : QObject(parent), d_ptr(new RenderClockPrivate(this)) {
        Q_D(AVQt::RenderClock);

        d->m_timer = new QTimer(this);
        d->m_timer->setTimerType(Qt::PreciseTimer);

        connect(d->m_timer, &QTimer::timeout, this, &RenderClock::timerTimeout);
    }

    RenderClock::RenderClock(RenderClockPrivate &p) : d_ptr(&p) {
        Q_D(AVQt::RenderClock);

        d->m_timer = new QTimer(this);
        d->m_timer->setTimerType(Qt::PreciseTimer);

        connect(d->m_timer, &QTimer::timeout, this, &RenderClock::timerTimeout);
    }

    RenderClock::~RenderClock() {
        Q_D(AVQt::RenderClock);

        d->m_timer->stop();
        delete d->m_timer;
    }

    void RenderClock::start() {
        Q_D(AVQt::RenderClock);
        bool shouldBe = false;
        if (d->m_active.compare_exchange_strong(shouldBe, true)) {
            qDebug("Starting clock");
            d->m_paused.store(false);
            d->m_timer->start();
        }
    }

    void RenderClock::stop() {
        Q_D(AVQt::RenderClock);

        bool shouldBe = true;
        qDebug("Clock active?: %s", (d->m_active.load() ? "true" : "false"));
        if (d->m_active.compare_exchange_strong(shouldBe, false)) {
            d->m_paused.store(false);
            qDebug("Stopping timer");
            d->m_timer->stop();
            d->m_timer->setInterval(d->m_interval);

            d->m_elapsedTime->invalidate();
            delete d->m_elapsedTime;
        }
    }

    void RenderClock::pause(bool paused) {
        Q_D(AVQt::RenderClock);

        bool shouldBe = !paused;
        if (d->m_paused.compare_exchange_strong(shouldBe, paused)) {
            if (paused) {
                if (d->m_elapsedTime) {
                    d->m_lastPauseTimestamp = d->m_elapsedTime->nsecsElapsed() / 1000;
                }
            } else {
                if (d->m_elapsedTime) {
                    d->m_pausedTime += (d->m_elapsedTime->nsecsElapsed() / 1000) - d->m_lastPauseTimestamp;
                }
            }
        }
    }

    void RenderClock::setInterval(int64_t interval) {
        Q_D(AVQt::RenderClock);

        if (interval > 0 && interval != d->m_interval.load()) {
            d->m_interval.store(interval);

            d->m_timer->setInterval(d->m_interval.load());
            d->m_timer->stop();
            d->m_timer->start();

            intervalChanged(interval);
        } else if (interval <= 0) {
            throw std::out_of_range("Only timer intervals above 0 allowed");
        }
    }

    int64_t RenderClock::getInterval() {
        Q_D(AVQt::RenderClock);
        return d->m_interval;
    }

    bool RenderClock::isActive() {
        Q_D(AVQt::RenderClock);
        return d->m_active.load();
    }

    void RenderClock::timerTimeout() {
        Q_D(AVQt::RenderClock);
//        qDebug("Clock timeout");
        if (!d->m_paused.load()) {
            if (!d->m_elapsedTime) {
                d->m_elapsedTime = new QElapsedTimer;
                d->m_elapsedTime->start();
                timeout(0);
            }
            qDebug("Timeout at: %lld", d->m_elapsedTime->nsecsElapsed() / 1000 - d->m_pausedTime);
            timeout((d->m_elapsedTime->nsecsElapsed() / 1000) - d->m_pausedTime);
        }
    }

    bool RenderClock::isPaused() {
        Q_D(AVQt::RenderClock);
        return d->m_paused.load();
    }

//    void RenderClock::connectCallback(QObject *obj, void(*function)()) {
//        Q_D(AVQt::RenderClock);
//        connect(d->m_timer, &QTimer::timeout, obj, function, Qt::QueuedConnection);
//    }
//
//    void RenderClock::connectCallback(void(*function)()) {
//        Q_D(AVQt::RenderClock);
//        connect(d->m_timer, &QTimer::timeout, function);
//    }
//
//    void RenderClock::disconnectCallback(QObject *obj) {
//        Q_D(AVQt::RenderClock);
//        d->m_timer->disconnect(obj);
//    }
}