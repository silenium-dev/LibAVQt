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

#include "IFrameSink.hpp"

#ifndef TRANSCODE_FRAMESAVER_H
#define TRANSCODE_FRAMESAVER_H


namespace AVQt {
    class FrameFileSaverPrivate;

    /*!
     * \class FrameFileSaver
     * \brief This class is a simple IFrameSink implementation that saves every n-th frame to BMP files
     * \since 0.1-dev
     * \ingroup frame-output
     *
     * While developing custom frame sources or filter, a simple image file output is very useful for debugging.
     * FrameFileSaver provides this feature with custom file prefixes for multiple instances in a single application
     *
     * \sa IFrameSink
     */
    class FrameFileSaver : public QObject, public IFrameSink {
    Q_OBJECT
        Q_INTERFACES(AVQt::IFrameSink)

        Q_DECLARE_PRIVATE(AVQt::FrameFileSaver)

    public:
        /*!
         * \brief Standard public constructor. Creates new FrameFileSaver with given frame interval and filename prefix
         * \param interval Every \c interval frames one frame will be saved
         * \param filePrefix Resulting filename will be (prefix)-(frame_index).bmp
         * \param parent Parent object for Qt meta object system
         */
        explicit FrameFileSaver(quint64 interval, QString filePrefix, QObject *parent = nullptr);

        /*!
         * \private
         */
        ~FrameFileSaver() Q_DECL_OVERRIDE;

        /*!
         * \brief Returns whether the frame saver is currently paused or not
         */
        bool isPaused() Q_DECL_OVERRIDE;

    public slots:

        /*!
         * \brief Does nothing
         */
        Q_INVOKABLE int init(int64_t duration) Q_DECL_OVERRIDE;

        /*!
         * \brief Does nothing
         */
        Q_INVOKABLE int deinit() Q_DECL_OVERRIDE;

        /*!
         * \brief Does nothing
         */
        Q_INVOKABLE int start() Q_DECL_OVERRIDE;

        /*!
         * \brief Does nothing
         */
        Q_INVOKABLE int stop() Q_DECL_OVERRIDE;

        /*!
         * \brief Sets paused flag, which can be retrieved with \c FrameFileSaver::isPaused() Q_DECL_OVERRIDE.
         *
         * When set to true, the frame handler won't save any frames until called again with paused set to false
         * \param pause new paused state
         */
        Q_INVOKABLE void pause(bool pause) Q_DECL_OVERRIDE;

        /*!
        * \brief Saves frame, if internal frame counter modulo given interval is 0
        * \param frame Frame in a QImage
        * \param entireDuration FFmpeg stream time base as rational number, if you don't know, what this means, you probably don't need it
        * \param framerate Source framerate as rational number
        */
        Q_INVOKABLE void onFrame(QImage frame, int64_t duration) Q_DECL_OVERRIDE;

        /*!
        * Does nothing except of freeing the frame
        */
        Q_INVOKABLE void onFrame(AVFrame *frame, AVRational timebase, AVRational framerate, int64_t duration) Q_DECL_OVERRIDE;

    signals:

        /*!
         * \brief Emitted after start() finished
         */
        void started() Q_DECL_OVERRIDE;

        /*!
         * \brief Emitted after stop() finished
         */
        void stopped() Q_DECL_OVERRIDE;

        /*!
         * \brief Emitted when paused flag changes
         * @param pause Current paused state
         */
        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        /*!
         * \brief Protected constructor for derived classes to set the private of the parent object
         * @param p private data structure
         */
        explicit FrameFileSaver(FrameFileSaverPrivate &p);

        /*!
         * \private
         */
        FrameFileSaverPrivate *d_ptr;
    };
}


#endif //TRANSCODE_FRAMESAVER_H
