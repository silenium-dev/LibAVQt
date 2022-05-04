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

#ifndef LIBAVQT_OPENALAUDIOOUTPUT_H
#define LIBAVQT_OPENALAUDIOOUTPUT_H

#include <QThread>

#include "output/IAudioSink.hpp"
#include "output/IFrameSink.hpp"

namespace AVQt {
    class OpenALAudioOutputPrivate;

    class OpenALAudioOutput : public QThread, public IAudioSink {
        Q_OBJECT
        Q_INTERFACES(AVQt::IAudioSink)

        Q_DECLARE_PRIVATE(AVQt::OpenALAudioOutput)

    public:
        explicit OpenALAudioOutput(QObject *parent = nullptr);

        OpenALAudioOutput(OpenALAudioOutput &&other) noexcept;

        bool isPaused() override;

    public slots:

        int init(AVQt::IAudioSource *source, int64_t duration, int sampleRate, AVSampleFormat sampleFormat, uint64_t channelLayout) override;

        int deinit(AVQt::IAudioSource *source) override;

        int start(AVQt::IAudioSource *source) override;

        int stop(AVQt::IAudioSource *source) override;

        void pause(AVQt::IAudioSource *source, bool pause) override;

        void onAudioFrame(AVQt::IAudioSource *source, AVFrame *frame, uint32_t duration) override;

        void enqueueAudioForFrame(qint64 pts, qint64 duration);

        void syncToOutput(AVQt::IFrameSink *output);

    signals:

        void started() Q_DECL_OVERRIDE;

        void stopped() Q_DECL_OVERRIDE;

        void paused(bool paused) Q_DECL_OVERRIDE;

    protected:
        [[maybe_unused]] explicit OpenALAudioOutput(OpenALAudioOutputPrivate &p);

        void run() Q_DECL_OVERRIDE;

        OpenALAudioOutputPrivate *d_ptr;
    };
}


#endif //LIBAVQT_OPENALAUDIOOUTPUT_H
