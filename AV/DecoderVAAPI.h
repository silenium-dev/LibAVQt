//
// Created by silas on 3/1/21.
//

#include "IFrameSource.h"

#include <QtCore>
#include <QtGui>

extern "C" {
#include <libavutil/avutil.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}


#ifndef TRANSCODE_DECODERVAAPI_H
#define TRANSCODE_DECODERVAAPI_H

namespace AV {
    class DecoderVAAPI : public QThread, public IFrameSource {
    Q_OBJECT
        Q_INTERFACES(AV::IFrameSource)
    public:
        explicit DecoderVAAPI(QIODevice *inputDevice, QObject *parent = nullptr);

        ~DecoderVAAPI() override = default;

        void run() override;

    public slots:
        Q_INVOKABLE int init() override;

        Q_INVOKABLE int deinit() override;

        Q_INVOKABLE int start() override;

        Q_INVOKABLE int stop() override;

        Q_INVOKABLE int pause(bool pause) override;

        Q_INVOKABLE int registerCallback(IFrameSink *frameSink, uint8_t type) override;

        Q_INVOKABLE int unregisterCallback(IFrameSink *frameSink) override;

    signals:

        void started() override;

        void stopped() override;

    private:
        static int readFromIO(void *opaque, uint8_t *buf, int bufSize);

        static int64_t seekIO(void *opaque, int64_t pos, int whence);

        uint8_t *pIOBuffer = nullptr;
        AVIOContext *pInputCtx = nullptr;
        AVFormatContext *pFormatCtx = nullptr;
        AVCodec *pVideoCodec = nullptr, *pAudioCodec = nullptr;
        AVCodecContext *pVideoCodecCtx = nullptr, *pAudioCodecCtx = nullptr;
        AVBufferRef *pDeviceCtx = nullptr;
        AVBufferRef *pFramesCtx = nullptr;
        AVFrame *pCurrentFrame = nullptr;
        AVFrame *pCurrentBGRAFrame = nullptr;

        int videoIndex = 0, audioIndex = 0;

        SwsContext *pSwsContext = nullptr;

        QIODevice *m_inputDevice;

        // Callback stuff
        QList<IFrameSink *> m_avfCallbacks, m_qiCallbacks;

        // Threading stuff
        std::atomic_bool isRunning = false;
        std::atomic_bool isPaused = false;
    };

}

#endif //TRANSCODE_DECODERVAAPI_H
