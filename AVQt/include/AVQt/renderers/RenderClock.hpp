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
