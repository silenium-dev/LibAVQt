// Copyright (c) 2022.
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

//
// Created by silas on 07.01.22.
//

#ifndef LIBAVQT_HWCONTEXTSYNC_HPP
#define LIBAVQT_HWCONTEXTSYNC_HPP

#include <QtCore>
#include <queue>

extern "C" {
#include <libavutil/hwcontext.h>
}

class ordered_lock {
    std::queue<std::condition_variable *> cvar;
    std::mutex cvar_lock;
    bool locked;

public:
    ordered_lock() : locked(false){};
    void lock() {
        std::unique_lock<std::mutex> acquire(cvar_lock);
        if (locked) {
            std::condition_variable signal;
            cvar.emplace(&signal);
            signal.wait(acquire);
        } else {
            locked = true;
        }
    }
    void unlock() {
        std::unique_lock<std::mutex> acquire(cvar_lock);
        if (cvar.empty()) {
            locked = false;
        } else {
            cvar.front()->notify_one();
            cvar.pop();
        }
    }
};

namespace AVQt {
    class HWContextSync {
    public:
        explicit HWContextSync(AVBufferRef *framesContext);

        AVBufferRef *getFramesContext() const;

        void lock();
        void unlock();

    private:
        AVBufferRef *framesContext;
        ordered_lock mutex;
    };
}// namespace AVQt


#endif//LIBAVQT_HWCONTEXTSYNC_HPP
