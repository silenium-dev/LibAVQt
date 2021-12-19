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

extern "C" {
#include <libavutil/samplefmt.h>
}

#ifndef LIBAVQT_IAUDIOSINK_H
#define LIBAVQT_IAUDIOSINK_H

struct AVFrame;

namespace AVQt {
    class IAudioSource;

    class IAudioSink {
    public:
        virtual ~IAudioSink() = default;

        virtual bool isPaused() = 0;

    public slots:
        Q_INVOKABLE virtual int
        init(IAudioSource *source, int64_t duration, int sampleRate, AVSampleFormat sampleFormat, uint64_t channelLayout) = 0;

        Q_INVOKABLE virtual int deinit(IAudioSource *source) = 0;

        Q_INVOKABLE virtual int start(IAudioSource *source) = 0;

        Q_INVOKABLE virtual int stop(IAudioSource *source) = 0;

        Q_INVOKABLE virtual void pause(IAudioSource *source, bool pause) = 0;

        Q_INVOKABLE virtual void onAudioFrame(IAudioSource *source, AVFrame *frame, uint32_t duration) = 0;

    signals:

        /*!
         * \brief Emitted when started
         */
        virtual void started() = 0;

        /*!
         * \brief Emitted when stopped
         */
        virtual void stopped() = 0;

        /*!
         * \brief Emitted when paused state changes
         * @param paused
         */
        virtual void paused(bool paused) = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IAudioSink, "AVQt::IAudioSink")

#endif //LIBAVQT_IAUDIOSINK_H
