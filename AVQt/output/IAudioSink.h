//
// Created by silas on 3/17/21.
//

#include <QObject>

extern "C" {
#include <libavutil/samplefmt.h>
}

#ifndef LIBAVQT_IAUDIOSINK_H
#define LIBAVQT_IAUDIOSINK_H

struct AVFrame;

namespace AVQt {
    class IAudioSink {
    public:
        virtual ~IAudioSink() = default;

        virtual bool isPaused() = 0;

    public slots:
        Q_INVOKABLE virtual int init(int64_t duration) = 0;

        Q_INVOKABLE virtual int deinit() = 0;

        Q_INVOKABLE virtual int start() = 0;

        Q_INVOKABLE virtual int stop() = 0;

        Q_INVOKABLE virtual void pause(bool pause) = 0;

        Q_INVOKABLE virtual void onAudioFrame(AVFrame *frame, uint32_t duration, AVSampleFormat sampleFormat) = 0;

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
         * @param pause
         */
        virtual void paused(bool pause) = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IAudioSink, "AVQt::IAudioSink")

#endif //LIBAVQT_IAUDIOSINK_H
