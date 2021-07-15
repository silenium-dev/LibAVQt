//
// Created by silas on 3/21/21.
//

#include "IFrameSink.h"
//#include <QtOpenGLWidgets>
#include <QtOpenGL>
#include <input/IFrameSource.h>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "HidingNonVirtualFunction"
extern "C" {
#include <libavutil/rational.h>
}

#ifndef LIBAVQT_OPENGLRENDERER_H
#define LIBAVQT_OPENGLRENDERER_H


namespace AVQt {
    class OpenGLRendererPrivate;

    class RenderClock;

    class OpenGLRenderer : public QOpenGLWidget, public IFrameSink {
    Q_OBJECT
//        Q_INTERFACES(AVQt::IFrameSink)

        Q_DECLARE_PRIVATE(AVQt::OpenGLRenderer)

    public:
        explicit OpenGLRenderer(QWidget *parent = nullptr);

        OpenGLRenderer(OpenGLRenderer &&other) noexcept;

        OpenGLRenderer(const OpenGLRenderer &) = delete;

        void operator=(const OpenGLRenderer &) = delete;

        ~OpenGLRenderer() noexcept Q_DECL_OVERRIDE;

        bool isPaused() Q_DECL_OVERRIDE;

        RenderClock *getClock();

    public slots:
        /*!
         * \brief Initialize frame sink (e.g. allocate contexts, open streams)
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int init(IFrameSource *source, AVRational framerate, int64_t duration) Q_DECL_OVERRIDE;

        /*!
         * \brief Clean up frame sink (e.g. free buffers, close files)
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int deinit(IFrameSource *source) Q_DECL_OVERRIDE;

        /*!
         * \brief Starts frame sink (e.g. start processing thread)
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int start(IFrameSource *source) Q_DECL_OVERRIDE;

        /*!
         * \brief Stops frame sink (e.g interrupt processing thread, flush buffers)
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int stop(IFrameSource *source) Q_DECL_OVERRIDE;

        /*!
         * \brief Sets paused flag, which can be retrieved with \c isPaused() Q_DECL_OVERRIDE.
         *
         * When Set to true, the frame sink should not process any frames, instead they should be freed immediately
         * \param pause New paused state
         */
        Q_INVOKABLE void pause(IFrameSource *source, bool pause) Q_DECL_OVERRIDE;

        /*!
         * \brief Image process method, is invoked in objects thread for every frame
         *
         * ***Disclaimer:*** Image processing should be moved to separate thread to prevent sources and filters from blocking
         * @param frame Source image in RGBA pixel format
         * @param entireDuration Source stream time base, if you don't know what this means, you probably don't want to use it.
         * @param framerate Source stream framerate
         */
//        Q_INVOKABLE void onFrame(QImage frame, int64_t duration) Q_DECL_OVERRIDE;

        /*!
         * \brief Image process method, is invoked in objects thread for every frame
         *
         * ***Disclaimer:*** Image processing should be moved to separate thread to prevent sources and filters from blocking
         * @param frame Source frame in any possible pixel format, processing code has to be able to handle every format
         * @param timebase Source stream time base, if you don't know what this means, you probably don't want to use it.
         * @param framerate Source stream framerate
         */
        Q_INVOKABLE void onFrame(IFrameSource *source, AVFrame *frame, int64_t frameDuration, AVBufferRef *pDeviceCtx) Q_DECL_OVERRIDE;

    signals:

        void started() Q_DECL_OVERRIDE;

        void stopped() Q_DECL_OVERRIDE;

        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        [[maybe_unused]] explicit OpenGLRenderer(OpenGLRendererPrivate &p);

        void initializeGL() Q_DECL_OVERRIDE;

        void paintGL() Q_DECL_OVERRIDE;

        void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

        void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

        OpenGLRendererPrivate *d_ptr;
    };
}


#endif //LIBAVQT_OPENGLRENDERER_H
#pragma clang diagnostic pop