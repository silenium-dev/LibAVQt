//
// Created by silas on 05.02.22.
//

#ifndef LIBAVQT_PIXELFORMAT_HPP
#define LIBAVQT_PIXELFORMAT_HPP

#include <QtCore/QMutex>
#include <QtCore/QObject>

#include <QtCore/QDebug>

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

namespace AVQt::common {
    struct PixelFormat {
    public:
        struct GPUPixelFormat {
            AVPixelFormat format;
            AVPixelFormat (*nativeFormat)(AVPixelFormat);
        };

        PixelFormat(AVPixelFormat cpuFormat, AVPixelFormat gpuFormat);

        PixelFormat() = default;
        PixelFormat(const PixelFormat &) = default;
        PixelFormat(PixelFormat &&) = default;
        PixelFormat &operator=(const PixelFormat &) = default;
        PixelFormat &operator=(PixelFormat &&) = default;

        [[nodiscard]] AVPixelFormat getGPUFormat() const;

        [[nodiscard]] AVPixelFormat getCPUFormat() const;

        bool setGPUFormat(AVPixelFormat format);

        bool setCPUFormat(AVPixelFormat format);

        [[nodiscard]] bool isValid() const;
        [[nodiscard]] bool isGPUFormat() const;
        [[nodiscard]] bool isSupportedBy(const QList<PixelFormat> &formats) const;

        [[nodiscard]] QString toString() const;

        static bool registerGPUFormat(GPUPixelFormat format);

        static bool unregisterGPUFormat(GPUPixelFormat format);
        static bool unregisterGPUFormat(AVPixelFormat format);

        [[nodiscard]] PixelFormat toNativeFormat() const;
        [[nodiscard]] PixelFormat toBaseFormat() const;

        friend QDebug operator<<(QDebug debug, const PixelFormat &pixelFormat);
        friend QDebug operator<<(QDebug debug, const PixelFormat *pixelFormat);
        friend std::ostream &operator<<(std::ostream &os, const PixelFormat &pixelFormat);
        friend std::ostream &operator<<(std::ostream &os, const PixelFormat *pixelFormat);

        friend bool operator==(const PixelFormat &lhs, const PixelFormat &rhs);
        friend bool operator!=(const PixelFormat &lhs, const PixelFormat &rhs);

        static void registerAllGPUFormats();

    private:
        AVPixelFormat m_gpuFormat{AV_PIX_FMT_NONE};
        AVPixelFormat m_cpuFormat{AV_PIX_FMT_NONE};

        class GPUPixelFormatRegistry;
    };
}// namespace AVQt::common


#endif//LIBAVQT_PIXELFORMAT_HPP
