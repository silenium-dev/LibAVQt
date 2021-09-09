//
// Created by silas on 3/28/21.
//

#include "../AudioDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <QtCore>

#ifndef LIBAVQT_AUDIODECODER_P_H
#define LIBAVQT_AUDIODECODER_P_H

namespace AVQt {
    class AudioDecoderPrivate : public QObject {
    Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::AudioDecoder)

    public:
        AudioDecoderPrivate(const AudioDecoderPrivate &) = delete;

        void operator=(const AudioDecoderPrivate &) = delete;

    private:
        explicit AudioDecoderPrivate(AudioDecoder *q) : q_ptr(q) {};

        AudioDecoder *q_ptr;

        QMutex m_inputQueueMutex{};
        QQueue<AVPacket *> m_inputQueue{};
        int64_t m_duration{0};

        AVCodecParameters *m_pCodecParams{nullptr};
        AVCodec *m_pCodec{nullptr};
        AVCodecContext *m_pCodecCtx{nullptr};
        AVRational m_timebase{0, 1};

        // Callback stuff
        QMutex m_cbListMutex{};
        QList<IAudioSink *> m_cbList{};

        // Threading stuff
        std::atomic_bool m_running{false};
        std::atomic_bool m_paused{false};

        friend class AudioDecoder;
    };
}

#endif //LIBAVQT_AUDIODECODER_P_H