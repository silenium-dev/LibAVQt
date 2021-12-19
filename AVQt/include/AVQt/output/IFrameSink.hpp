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

#include <QObject>

#ifndef TRANSCODE_IFRAMESINK_H
#define TRANSCODE_IFRAMESINK_H

struct AVFrame;
struct AVRational;
struct AVBufferRef;

namespace AVQt {
    /*!
     * \private
     */
    class IFrameSource;

    /*!
     * \interface IFrameSink
     * \brief Base class for every frame sink
     *
     * Every subclass of IFrameSink represents the end of a video processing pipeline. This can be e.g. an encoder, an image file output or a renderer
     */
    class IFrameSink {
    public:
        /*!
         * \private
         */
        virtual ~IFrameSink() = default;

        /*!
         * \brief Retrieve paused state of frame sink
         * @return Paused state
         */
        virtual bool isPaused() = 0;

    public slots:

        /*!
         * \brief Initialize frame sink (e.g. allocate contexts, open streams)
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int init(AVQt::IFrameSource *source, AVRational framerate, int64_t duration) = 0;

        /*!
         * \brief Clean up frame sink (e.g. free buffers, close files)
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int deinit(AVQt::IFrameSource *source) = 0;

        /*!
         * \brief Starts frame sink (e.g. start processing thread)
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int start(AVQt::IFrameSource *source) = 0;

        /*!
         * \brief Stops frame sink (e.g interrupt processing thread, flush buffers)
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int stop(AVQt::IFrameSource *source) = 0;

        /*!
         * \brief Sets paused flag, which can be retrieved with \c isPaused() Q_DECL_OVERRIDE.
         *
         * When Set to true, the frame sink should not process any frames, instead they should be freed immediately
         * \param pause New paused state
         */
        Q_INVOKABLE virtual void pause(AVQt::IFrameSource *source, bool pause) = 0;
//
//        /*!
//         * \brief Image process method, is invoked in objects thread for every frame
//         *
//         * ***Disclaimer:*** Image processing should be moved to separate thread to prevent sources and filters from blocking
//         * @param frame Source image in RGBA pixel format
//         * @param entireDuration Source stream time base, if you don't know what this means, you probably don't want to use it.
//         * @param framerate Source stream framerate
//         */
//        Q_INVOKABLE virtual void onFrame(QImage frame, int64_t duration) = 0;

        /*!
         * \brief Image process method, is invoked in source's thread for every frame
         *
         * ***Disclaimer:*** Image processing should be moved to separate thread to prevent sources and filters from blocking
         * @param source Frame source for distinguishing between multiple frame sources (e.g. for saving frames to file)
         * @param frame Source frame in any possible pixel format, processing code has to be able to handle every format
         * @param timebase Source stream time base, if you don't know what this means, you probably don't need to use it.
         * @param framerate Source stream framerate
         * @param duration Source frame presentation duration (inverse of framerate)
         */
        Q_INVOKABLE virtual void onFrame(AVQt::IFrameSource *source, AVFrame *frame, int64_t frameDuration, AVBufferRef *pDeviceCtx) = 0;

    signals:

        /*!
         * \brief Emitted when started
         */
        virtual void started() = 0;

        /*!
         * \brief Emitted when stopped
         */
        virtual void stopped() = 0;

        /*!
         * \brief Emitted when paused state changes
         * @param pause
         */
        virtual void paused(bool pause) = 0;

        /*!
         * \brief Emitted when frame sink starts processing a frame
         * @param pts Presentation timestamp of the frame being processed
         * @param duration Duration in microseconds of the frame being processed
         */
        virtual void frameProcessingStarted(qint64 pts, qint64 duration) = 0;

        /*!
         * \brief Emitted when frame sink finished processing a frame
         * @param pts Presentation timestamp of the frame that got processed
         * @param duration Duration in microseconds of the frame that got processed
         */
        virtual void frameProcessingFinished(qint64 pts, qint64 duration) = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IFrameSink, "IFrameSink")

#endif //TRANSCODE_IFRAMESINK_H
