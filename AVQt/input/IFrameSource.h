//
// Created by silas on 3/1/21.
//

#include <QObject>

#ifndef TRANSCODE_IFRAMESOURCE_H
#define TRANSCODE_IFRAMESOURCE_H

namespace AVQt {
    class IFrameSink;

    class IFrameSource {
    public:
        enum : int8_t {
            CB_NONE = -1,
            CB_QIMAGE = 0,
            CB_AVFRAME = 1
        };

//        static constexpr uint8_t CB_AVFRAME = 0x01;
//        static constexpr uint8_t CB_QIMAGE = 0x02;

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

        virtual void started() = 0;

        virtual void stopped() = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IFrameSource, "IFrameSource")

#endif //TRANSCODE_IFRAMESOURCE_H
