// Copyright (c) 2021.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "output/IFrameSink.hpp"
#include <QtOpenGL>
#include "AVQt/input/IFrameSource.hpp"

extern "C" {
#include <libavutil/rational.h>
}

#ifndef LIBAVQT_OPENGLRENDERER_H
#define LIBAVQT_OPENGLRENDERER_H


namespace AVQt {
    class OpenGLRendererPrivate;

    class RenderClock;

    class OpenGLRendererOld : public QObject, public QOpenGLFunctions, public IFrameSink {
        Q_OBJECT
        Q_INTERFACES(AVQt::IFrameSink)
        Q_DECLARE_PRIVATE(AVQt::OpenGLRenderer)
        Q_DISABLE_COPY(OpenGLRendererOld)

    public:
        explicit OpenGLRendererOld(QObject *parent = nullptr);

        OpenGLRendererOld(OpenGLRendererOld &&other) noexcept;
        //
        //        OpenGLRendererOld(const OpenGLRendererOld &) = delete;
        //
        //        void operator=(const OpenGLRendererOld &) = delete;

        ~OpenGLRendererOld() noexcept Q_DECL_OVERRIDE;

        bool isPaused() Q_DECL_OVERRIDE;

        void initializeGL(QOpenGLContext *context);

        void paintGL();

        void paintFBO(QOpenGLFramebufferObject *fbo);

        void cleanupGL();

        QSize getFrameSize();

        bool isUpdateRequired();

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

        void frameProcessingStarted(qint64 pts, qint64 duration) override;

        void frameProcessingFinished(qint64 pts, qint64 duration) override;

    protected:
        [[maybe_unused]] explicit OpenGLRendererOld(OpenGLRendererPrivate &p);

        OpenGLRendererPrivate *d_ptr;

        // Platform-dependent
        void initializePlatformAPI();
        void initializeInterop();
        void updatePixelFormat();
        void mapFrame();
    };
}// namespace AVQt


#endif//LIBAVQT_OPENGLRENDERER_H
