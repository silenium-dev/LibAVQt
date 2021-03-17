/*!
 * \private
 * \internal
 */
#include "../OpenGLWidgetRenderer.h"

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

#ifndef TRANSCODE_OPENGLWIDGETRENDERER_P_H
#define TRANSCODE_OPENGLWIDGETRENDERER_P_H


namespace AVQt {
    /*!
     * \private
     */
    class OpenGLWidgetRendererPrivate {
    private:
        explicit OpenGLWidgetRendererPrivate(OpenGLWidgetRenderer *q) : q_ptr(q) {}

        void resizeFrameRects();

        OpenGLWidgetRenderer *q_ptr;
        QMutex m_currentImageMutex;
        QRect m_currentImageRect;
        QImage m_currentImage;

//        QQueue<QPair<QRect, QImage>> m_imageInputQueue;
        QMutex m_imageInputQueueMutex;
        QQueue<QVariantList> m_imageInputQueue;

        QMutex m_imageRenderQueueMutex;
        QQueue<QVariantList> m_imageRenderQueue;

        QFuture<void> m_scalingFuture;

        SwsContext *m_pSwsContext = nullptr;

        AVPixelFormat m_swsCtxFormat = AV_PIX_FMT_NONE;
        QTimer *m_updateTimer = nullptr;
        quint32 m_currentTimeout = 0;

        std::atomic_bool m_isPaused = false;
        std::atomic_bool m_isRunning = false;

        std::chrono::time_point<std::chrono::high_resolution_clock> m_lastNewFrame = std::chrono::high_resolution_clock::now();

        friend class OpenGLWidgetRenderer;
    };
}

#endif //TRANSCODE_OPENGLWIDGETRENDERER_P_H
