//
// Created by silas on 3/1/21.
//

#include "IFrameSink.h"

#include <QtCore>

#ifndef TRANSCODE_IFRAMESOURCE_H
#define TRANSCODE_IFRAMESOURCE_H

namespace AV {
    class IFrameSource {
    public:
        static constexpr uint8_t CB_AVFRAME = 0x01;
        static constexpr uint8_t CB_QIMAGE = 0x02;

        virtual ~IFrameSource() = default;

    public slots:
        Q_INVOKABLE virtual int init() = 0;

        Q_INVOKABLE virtual int deinit() = 0;

        Q_INVOKABLE virtual int start() = 0;

        Q_INVOKABLE virtual int stop() = 0;

        Q_INVOKABLE virtual int pause(bool pause) = 0;

        Q_INVOKABLE virtual int registerCallback(IFrameSink *frameSink, uint8_t type) = 0;

        Q_INVOKABLE virtual int unregisterCallback(IFrameSink *frameSink) = 0;

    signals:

        virtual void started() {};

        virtual void stopped() {};
    };
}

Q_DECLARE_INTERFACE(AV::IFrameSource, "IFrameSource")

#endif //TRANSCODE_IFRAMESOURCE_H
