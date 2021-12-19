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

#include <QObject>

#ifndef LIBAVQT_IAUDIOSOURCE_H
#define LIBAVQT_IAUDIOSOURCE_H

struct AVFrame;

namespace AVQt {
    class IAudioSink;

    class IAudioSource {
    public:
        virtual ~IAudioSource() = default;

        virtual bool isPaused() = 0;

        Q_INVOKABLE virtual qint64 registerCallback(AVQt::IAudioSink *callback) = 0;

        Q_INVOKABLE virtual qint64 unregisterCallback(AVQt::IAudioSink *callback) = 0;

    public slots:
        Q_INVOKABLE virtual int init() = 0;

        Q_INVOKABLE virtual int deinit() = 0;

        Q_INVOKABLE virtual int start() = 0;

        Q_INVOKABLE virtual int stop() = 0;

        Q_INVOKABLE virtual void pause(bool isPaused) = 0;

    signals:

        virtual void started() = 0;

        virtual void stopped() = 0;

        virtual void paused(bool pause) = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IAudioSource, "AVQt::IAudioSink")

#endif //LIBAVQT_IAUDIOSOURCE_H
