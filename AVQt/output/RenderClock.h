//
// Created by silas on 3/31/21.
//

#include <QtCore>

#ifndef LIBAVQT_RENDERCLOCK_H
#define LIBAVQT_RENDERCLOCK_H

namespace AVQt {
    class RenderClockPrivate;

    class RenderClock : public QObject {
    Q_OBJECT

        Q_DECLARE_PRIVATE(AVQt::RenderClock)

    public:
        explicit RenderClock(QObject *parent = nullptr);

        ~RenderClock();

        void start();

        void stop();

        void pause(bool paused);

        void setInterval(int64_t interval);

        int64_t getInterval();

        bool isActive();

//        void connectCallback(QObject *obj, void(*function)());
//
//        void connectCallback(void(*function)());
//
//        void disconnectCallback(QObject *obj);

    signals:

        void intervalChanged(int64_t interval);

        void timeout();

    protected:
        explicit RenderClock(RenderClockPrivate &p);

        RenderClockPrivate *d_ptr;

    };
}


#endif //LIBAVQT_RENDERCLOCK_H
