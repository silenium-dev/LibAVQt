//
// Created by silas on 05.02.22.
//

#include "common/PixelFormat.hpp"

AVPixelFormat OneInterleavedPlanePixelFormatOf(AVPixelFormat format) {
    switch (format) {
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_YUV420P:
            return AV_PIX_FMT_NV12;
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_YUV420P10:
            return AV_PIX_FMT_P010;
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
            return AV_PIX_FMT_P016;
        default:
            return AV_PIX_FMT_NONE;
    }
}

AVPixelFormat FullFormatPixelFormatOf(AVPixelFormat format) {
    switch (format) {
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_YUV420P:
            return AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_YUV420P10:
            return AV_PIX_FMT_YUV420P10;
        case AV_PIX_FMT_YUV420P12:
            return AV_PIX_FMT_YUV420P12;
        case AV_PIX_FMT_YUV420P14:
            return AV_PIX_FMT_YUV420P14;
        case AV_PIX_FMT_P016:
        case AV_PIX_FMT_YUV420P16:
            return AV_PIX_FMT_YUV420P16;
        default:
            return AV_PIX_FMT_NONE;
    }
}

namespace AVQt::common {
    class PixelFormat::GPUPixelFormatRegistry {
    public:
        static GPUPixelFormatRegistry &getInstance();
        QMutex s_gpuFormatsMutex{};
        QMap<AVPixelFormat, GPUPixelFormat> s_gpuFormats{};
    };

    bool PixelFormat::registerGPUFormat(GPUPixelFormat format) {
        QMutexLocker locker(&GPUPixelFormatRegistry::getInstance().s_gpuFormatsMutex);
        if (GPUPixelFormatRegistry::getInstance().s_gpuFormats.contains(format.format)) {
            return false;
        }
        GPUPixelFormatRegistry::getInstance().s_gpuFormats.insert(format.format, format);
        return true;
    }

    bool PixelFormat::unregisterGPUFormat(GPUPixelFormat format) {
        QMutexLocker locker(&GPUPixelFormatRegistry::getInstance().s_gpuFormatsMutex);
        return GPUPixelFormatRegistry::getInstance().s_gpuFormats.remove(format.format);
    }

    bool PixelFormat::unregisterGPUFormat(AVPixelFormat format) {
        QMutexLocker locker(&GPUPixelFormatRegistry::getInstance().s_gpuFormatsMutex);
        return GPUPixelFormatRegistry::getInstance().s_gpuFormats.remove(format);
    }

    void PixelFormat::registerAllGPUFormats() {
        std::atomic_bool registered{false};
        bool shouldBe = false;
        if (registered.compare_exchange_strong(shouldBe, true)) {
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_VAAPI_VLD, .nativeFormat = &OneInterleavedPlanePixelFormatOf});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_VAAPI_IDCT, .nativeFormat = &OneInterleavedPlanePixelFormatOf});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_VAAPI_MOCO, .nativeFormat = &OneInterleavedPlanePixelFormatOf});

            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_D3D11VA_VLD, .nativeFormat = &OneInterleavedPlanePixelFormatOf});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_D3D11, .nativeFormat = &OneInterleavedPlanePixelFormatOf});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_DXVA2_VLD, .nativeFormat = &OneInterleavedPlanePixelFormatOf});

            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_MMAL, .nativeFormat = nullptr});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_MEDIACODEC, .nativeFormat = nullptr});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_QSV, .nativeFormat = &FullFormatPixelFormatOf});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_VDPAU, .nativeFormat = nullptr});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_VIDEOTOOLBOX, .nativeFormat = nullptr});

            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_CUDA, .nativeFormat = nullptr});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_DRM_PRIME, .nativeFormat = nullptr});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_OPENCL, .nativeFormat = nullptr});
            AVQt::common::PixelFormat::registerGPUFormat({.format = AV_PIX_FMT_VULKAN, .nativeFormat = nullptr});
        }
    }

    PixelFormat::PixelFormat(AVPixelFormat cpuFormat, AVPixelFormat gpuFormat) {
        if (cpuFormat != AV_PIX_FMT_NONE && GPUPixelFormatRegistry::getInstance().s_gpuFormats.contains(cpuFormat)) {
            qWarning("PixelFormat: CPU format %s is a GPU format", av_get_pix_fmt_name(cpuFormat));
        }
        if (gpuFormat != AV_PIX_FMT_NONE && !GPUPixelFormatRegistry::getInstance().s_gpuFormats.contains(gpuFormat)) {
            qWarning("PixelFormat: GPU format %s is not a registered GPU format", av_get_pix_fmt_name(gpuFormat));
        }
        m_cpuFormat = cpuFormat;
        m_gpuFormat = gpuFormat;
    }

    bool PixelFormat::isValid() const {
        return (m_gpuFormat == AV_PIX_FMT_NONE || GPUPixelFormatRegistry::getInstance().s_gpuFormats.contains(m_gpuFormat)) &&
               (m_cpuFormat == AV_PIX_FMT_NONE || !GPUPixelFormatRegistry::getInstance().s_gpuFormats.contains(m_cpuFormat));
    }

    bool PixelFormat::isGPUFormat() const {
        return m_gpuFormat != AV_PIX_FMT_NONE && GPUPixelFormatRegistry::getInstance().s_gpuFormats.contains(m_gpuFormat);
    }

    bool PixelFormat::setCPUFormat(AVPixelFormat format) {
        if (format == m_cpuFormat || GPUPixelFormatRegistry::getInstance().s_gpuFormats.contains(format)) {
            return false;
        }
        m_cpuFormat = format;
        return true;
    }

    bool PixelFormat::setGPUFormat(AVPixelFormat format) {
        if (format == m_gpuFormat || !GPUPixelFormatRegistry::getInstance().s_gpuFormats.contains(format)) {
            return false;
        }
        m_gpuFormat = format;
        return true;
    }

    AVPixelFormat PixelFormat::getCPUFormat() const {
        return m_cpuFormat;
    }

    AVPixelFormat PixelFormat::getGPUFormat() const {
        return m_gpuFormat;
    }

    PixelFormat PixelFormat::toNativeFormat() const {
        if (GPUPixelFormatRegistry::getInstance().s_gpuFormats.contains(m_gpuFormat)) {
            auto gpuFormatInfo = GPUPixelFormatRegistry::getInstance().s_gpuFormats.value(m_gpuFormat);
            if (gpuFormatInfo.nativeFormat) {
                return {gpuFormatInfo.nativeFormat(m_cpuFormat), m_gpuFormat};
            }
        }
        return {m_cpuFormat, m_gpuFormat};
    }

    PixelFormat PixelFormat::toBaseFormat() const {
        return {FullFormatPixelFormatOf(m_cpuFormat), AV_PIX_FMT_NONE};
    }

    QString PixelFormat::toString() const {
        return QString("GPU: %1, CPU: %2").arg(av_get_pix_fmt_name(m_gpuFormat), av_get_pix_fmt_name(m_cpuFormat));
    }

    QDebug operator<<(QDebug debug, const PixelFormat &pixelFormat) {
        debug.nospace() << pixelFormat.toString();
        return debug.space();
    }
    QDebug operator<<(QDebug debug, const PixelFormat *pixelFormat) {
        debug.nospace() << pixelFormat->toString();
        return debug.space();
    }
    std::ostream &operator<<(std::ostream &os, const PixelFormat &pixelFormat) {
        os << pixelFormat.toString().toStdString();
        return os;
    }
    std::ostream &operator<<(std::ostream &os, const PixelFormat *pixelFormat) {
        os << pixelFormat->toString().toStdString();
        return os;
    }

    bool operator==(const PixelFormat &lhs, const PixelFormat &rhs) {
        return lhs.m_cpuFormat == rhs.m_cpuFormat && lhs.m_gpuFormat == rhs.m_gpuFormat;
    }

    bool operator!=(const PixelFormat &lhs, const PixelFormat &rhs) {
        return lhs.m_cpuFormat != rhs.m_cpuFormat || lhs.m_gpuFormat != rhs.m_gpuFormat;
    }

    PixelFormat::GPUPixelFormatRegistry &PixelFormat::GPUPixelFormatRegistry::getInstance() {
        static PixelFormat::GPUPixelFormatRegistry instance;
        return instance;
    }
}// namespace AVQt::common

static_block {
    AVQt::common::PixelFormat::registerAllGPUFormats();
};
