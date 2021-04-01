//
// Created by silas on 3/31/21.
//

#include "private/RenderClock_p.h"
#include "RenderClock.h"

namespace AVQt {
    RenderClock::RenderClock(QObject *parent) : QObject(parent), d_ptr(new RenderClockPrivate(this)) {
        Q_D(AVQt::RenderClock);

        d->m_timer = new QTimer(this);
        connect(d->m_timer, &QTimer::timeout, [this] {
            timeout();
        });
    }

    RenderClock::RenderClock(RenderClockPrivate &p) : d_ptr(&p) {
        Q_D(AVQt::RenderClock);

        d->m_timer = new QTimer(this);
    }

    RenderClock::~RenderClock() {
        Q_D(AVQt::RenderClock);
    }


    void RenderClock::start() {
        Q_D(AVQt::RenderClock);

        d->m_timer->start();
    }

    void RenderClock::stop() {
        Q_D(AVQt::RenderClock);

        d->m_timer->stop();
        d->m_timer->setInterval(d->m_interval);
    }

    void RenderClock::pause(bool paused) {
        Q_D(AVQt::RenderClock);

        d->m_timer->stop();
    }

    void RenderClock::setInterval(int64_t interval) {
        Q_D(AVQt::RenderClock);

        if (interval > 0 && interval != d->m_interval) {
            d->m_interval = interval;

            d->m_timer->setInterval(d->m_interval);

            intervalChanged(interval);
        } else {
            throw std::out_of_range("Only timer intervals above 0 allowed");
        }
    }

    int64_t RenderClock::getInterval() {
        Q_D(AVQt::RenderClock);
        return d->m_interval;
    }

    bool RenderClock::isActive() {
        Q_D(AVQt::RenderClock);
        return d->m_timer->isActive();
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