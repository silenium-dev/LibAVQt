/*!
 * \private
 */

#include "../EncoderVAAPI.h"
#include "../../input/IFrameSource.h"

#include <QtCore>

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

#ifndef TRANSCODE_ENCODERVAAPIPRIVATE_H
#define TRANSCODE_ENCODERVAAPIPRIVATE_H


namespace AVQt {
    /*!
     * \private
     */
    struct EncoderVAAPIPrivate {
        explicit EncoderVAAPIPrivate(EncoderVAAPI *q) : q_ptr(q) {}

        EncoderVAAPI *q_ptr;

        /*!
         * \private
         */
        struct Frame {
            AVFrame *frame;
            AVRational timeBase, framerate;
        };

        static int writeToIO(void *opaque, uint8_t *buf, int bufSize);

        [[maybe_unused]] static AVPixelFormat getPixelFormat(AVCodecContext *pCodecCtx, const AVPixelFormat *formats);

        [[maybe_unused]] static int64_t frameNumberToPts(uint64_t frameNumber, AVRational frameRate, AVRational timeBase);

        uint8_t *pIOBuffer = nullptr;
        AVIOContext *pOutputCtx = nullptr;
        AVFormatContext *pFormatCtx = nullptr;
        AVCodec *pVideoCodec = nullptr, *pAudioCodec = nullptr;
        AVCodecContext *pVideoCodecCtx = nullptr, *pAudioCodecCtx = nullptr;
        AVStream *pVideoStream = nullptr, *pAudioStream = nullptr;
        AVBufferRef *pDeviceCtx = nullptr;
        AVBufferRef *pFramesCtx = nullptr;
        AVFrame *pCurrentFrame = nullptr;
        AVFrame *pHWFrame = nullptr;

        EncoderVAAPI::VIDEO_CODEC m_videoCodec = EncoderVAAPI::VIDEO_CODEC::H264;
        EncoderVAAPI::AUDIO_CODEC m_audioCodec = EncoderVAAPI::AUDIO_CODEC::AAC;
        int m_bitrate = 0, m_width = 0, m_height = 0;
        AVRational m_frameRate = av_make_q(0, 1);
        std::atomic_uint64_t m_frameNumber = 0;

        SwsContext *pSwsContext = nullptr;

        QIODevice *m_outputDevice = nullptr;
        QQueue<Frame *> m_frameQueue;
        QMutex m_frameQueueMutex;

        int8_t m_cbType = IFrameSource::CB_NONE;
        QMutex m_cbTypeMutex;

        // Threading stuff
        std::atomic_bool m_isRunning = false;
        std::atomic_bool m_isPaused = false;
        std::atomic_bool m_isFirstFrame = true;
    };
}

#endif //TRANSCODE_ENCODERVAAPIPRIVATE_H
