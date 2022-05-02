#ifndef LIBAVQT_IAUDIOOUTPUTIMPL_HPP
#define LIBAVQT_IAUDIOOUTPUTIMPL_HPP

#include "common/Platform.hpp"
#include "communication/AudioPadParams.hpp"

#include <QtCore/QObject>

extern "C" {
#include <libavutil/frame.h>
}

namespace AVQt::api {
    class IAudioOutputImpl {
    public:
        virtual ~IAudioOutputImpl() = default;
        virtual bool open(const communication::AudioPadParams &params) = 0;
        virtual void close() = 0;
        virtual void write(const std::shared_ptr<AVFrame> &frame) = 0;
        virtual void resetBuffer() = 0;
    };

    struct AudioOutputImplInfo {
        QString name;
        QMetaObject metaObject;
        QList<common::Platform::Platform_t> supportedPlatforms;

        /**
         * List of supported audio formats. If empty, all formats are supported.
         */
        QList<common::AudioFormat> supportedFormats;
        bool (*isSupported)();
    };
}// namespace AVQt::api

Q_DECLARE_INTERFACE(AVQt::api::IAudioOutputImpl, "avqt.api.IAudioOutputImpl")

#endif//LIBAVQT_IAUDIOOUTPUTIMPL_HPP
