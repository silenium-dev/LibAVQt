// Copyright (c) 2021.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "renderers/RenderClock.hpp"
#include "renderers/private/RenderClock_p.hpp"

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

    RenderClock::RenderClock(RenderClock &&other) noexcept: d_ptr(other.d_ptr) {
        other.d_ptr = nullptr;
        d_ptr->q_ptr = this;
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
            d->m_elapsedTime = new QElapsedTimer;
            d->m_elapsedTime->start();
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
            d->m_lastPauseTimestamp = 0;
            delete d->m_elapsedTime;
        }
    }

    void RenderClock::pause(bool paused) {
        Q_D(AVQt::RenderClock);

        bool shouldBe = !paused;
        if (d->m_paused.compare_exchange_strong(shouldBe, paused)) {
            if (paused) {
                if (d->m_elapsedTime) {
                    d->m_lastPauseTimestamp += d->m_elapsedTime->nsecsElapsed() / 1000;
                    d->m_elapsedTime->invalidate();
                }
            } else {
                if (d->m_elapsedTime && d->m_lastPauseTimestamp >= 0) {
                    d->m_elapsedTime->restart();
                }
            }
        }
    }

    void RenderClock::setInterval(int interval) {
        Q_D(AVQt::RenderClock);

        if (interval > 0 && interval != d->m_interval) {
            d->m_interval.store(interval);

            d->m_timer->setInterval(d->m_interval);
            d->m_timer->stop();
            d->m_timer->start();

            intervalChanged(interval);
        } else if (interval <= 0) {
            throw std::out_of_range("Only positive timer intervals are allowed");
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
            }
            if (!d->m_elapsedTime->isValid()) {
                timeout(d->m_lastPauseTimestamp);
            } else {
                timeout(d->m_lastPauseTimestamp + d->m_elapsedTime->nsecsElapsed() / 1000);
            }
        }
    }

    bool RenderClock::isPaused() {
        Q_D(AVQt::RenderClock);
        return d->m_paused.load();
    }

    int64_t RenderClock::getTimestamp() {
        Q_D(AVQt::RenderClock);
        if (!d->m_paused && d->m_elapsedTime && d->m_elapsedTime->isValid()) {
            qDebug("Elapsed: %lld us", d->m_elapsedTime->nsecsElapsed());
            return d->m_lastPauseTimestamp + d->m_elapsedTime->nsecsElapsed() / 1000;
        } else {
            qDebug("Paused or invalid elapsed time");
            return d->m_lastPauseTimestamp;
        }
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
