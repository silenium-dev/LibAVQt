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

        RenderClock(RenderClock &&other) noexcept;

        RenderClock(const RenderClock &) = delete;

        void operator=(const RenderClock &) = delete;

        ~RenderClock() override;

        void setInterval(int interval);

        int64_t getInterval();

        int64_t getTimestamp();

        bool isActive();

        bool isPaused();

    public slots:
        Q_INVOKABLE void start();

        Q_INVOKABLE void stop();

        Q_INVOKABLE void pause(bool paused);

//        void connectCallback(QObject *obj, void(*function)());
//
//        void connectCallback(void(*function)());
//
//        void disconnectCallback(QObject *obj);

    signals:

        void intervalChanged(int64_t interval);

        void timeout(qint64 elapsed);

    protected:
        explicit RenderClock(RenderClockPrivate &p);

        RenderClockPrivate *d_ptr;

    private slots:

        void timerTimeout();

    };
}


#endif //LIBAVQT_RENDERCLOCK_H