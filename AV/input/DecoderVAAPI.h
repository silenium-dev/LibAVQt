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
    struct DecoderVAAPIPrivate;

    class DecoderVAAPI : public QThread, public IFrameSource {
    Q_OBJECT
        Q_INTERFACES(AV::IFrameSource)

        Q_DECLARE_PRIVATE(AV::DecoderVAAPI)

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

    protected:
        explicit DecoderVAAPI(DecoderVAAPIPrivate &p);

        DecoderVAAPIPrivate *d_ptr;
    };

}

#endif //TRANSCODE_DECODERVAAPI_H
