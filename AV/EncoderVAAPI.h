//
// Created by silas on 3/3/21.
//

#include "IFrameSink.h"

#include <QtCore>
#include <QtGui>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h> // NOLINT(modernize-deprecated-headers)
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}

#ifndef TRANSCODE_ENCODERVAAPI_H
#define TRANSCODE_ENCODERVAAPI_H

namespace AV {
    class EncoderVAAPI : public QThread, public IFrameSink {
    Q_OBJECT
        Q_INTERFACES(AV::IFrameSink)
    public:
        enum class VIDEO_CODEC {
            H264, HEVC, VP9
        };

        enum class AUDIO_CODEC {
            AAC, OGG, OPUS
        };

        explicit EncoderVAAPI(QIODevice *outputDevice, VIDEO_CODEC videoCodec, AUDIO_CODEC audioCodec, int width, int height,
                              AVRational framerate, int bitrate,
                              QObject *parent = nullptr);

        ~EncoderVAAPI() override;

        void run() override;

    public slots:
        Q_INVOKABLE int init() override;

        Q_INVOKABLE int deinit() override;

        Q_INVOKABLE int start() override;

        Q_INVOKABLE int stop() override;

        Q_INVOKABLE int pause(bool pause) override;

        Q_INVOKABLE void onFrame(QImage frame, AVRational timebase, AVRational framerate) override;

        Q_INVOKABLE void onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) override;

    signals:

        void started() override;

        void stopped() override;

    private:
        struct Frame {
            AVFrame *frame;
            AVRational timeBase, framerate;
        };

        static int writeToIO(void *opaque, uint8_t *buf, int bufSize);

        static AVPixelFormat getPixelFormat(AVCodecContext *pCodecCtx, const AVPixelFormat *formats);

        static int64_t frameNumberToPts(uint64_t frameNumber, AVRational frameRate, AVRational timeBase);

        uint8_t *pIOBuffer = nullptr;
        AVIOContext *pOutputCtx = nullptr;
        AVFormatContext *pFormatCtx = nullptr;
        AVCodec *pVideoCodec = nullptr, *pAudioCodec = nullptr;
        AVCodecContext *pVideoCodecCtx = nullptr, *pAudioCodecCtx = nullptr;
        AVStream *pVideoStream = nullptr, *pAudioStream = nullptr;
        AVBufferRef *pDeviceCtx = nullptr;
        AVBufferRef *pFramesCtx = nullptr;
        AVFrame *pCurrentFrame = nullptr;
        AVFrame *pCurrentBGRAFrame = nullptr;
        AVFrame *pHWFrame;

        int m_videoIndex = 0, m_audioIndex = 0;
        VIDEO_CODEC m_videoCodec;
        AUDIO_CODEC m_audioCodec;
        int m_bitrate, m_width, m_height;
        int64_t m_dtsOffset = -1;
        AVRational m_frameRate;
        std::atomic_uint64_t m_frameNumber = 0;

        SwsContext *pSwsContext = nullptr;

        QIODevice *m_outputDevice;
        QQueue<Frame *> m_frameQueue;
        QMutex m_frameQueueMutex;

        uint8_t m_cbType = 0;
        QMutex m_cbTypeMutex;

        // Threading stuff
        std::atomic_bool isRunning = false;
        std::atomic_bool isPaused = false;
        std::atomic_bool m_isFirstFrame = true;
    };
}

#endif //TRANSCODE_ENCODERVAAPI_H
