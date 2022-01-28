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

#include "AVQt/renderers/RenderClock.hpp"

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
