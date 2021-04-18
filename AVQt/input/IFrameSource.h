//
// Created by silas on 3/1/21.
//

#include <QObject>

#ifndef TRANSCODE_IFRAMESOURCE_H
#define TRANSCODE_IFRAMESOURCE_H

namespace AVQt {
    class IFrameSink;

    /*!
     * \interface IFrameSource
     * \brief Base class for every frame sink
     *
     * All subclasses of IFrameSource represent a possible start of a frame processing pipeline (e.g. a decoder, a desktop/window capture, camera).
     */
    class IFrameSource {
    public:
        /*!
         * \enum CB_TYPE
         * \brief Used to define accepted callback types of a frame source or filter.
         *
         * Can be linked with bitwise or to set multiple types
         */
        enum CB_TYPE : int8_t {
            /*!
             * \private
             */
            CB_NONE = -1,

            /*!
             * \brief Callback type: QImage. When registered with this type, the onFrame(QImage, ...) method will be called
             */
            CB_QIMAGE = 0b00000001,

            /*!
             * \brief Callback type: AVFrame. When registered with this type, the onFrame(AVFrame, ...) method will be called.
             *
             * ***Important:*** The AVFrame can be every possible software pixel format, so the frame sink/filter has to be able to
             * convert the pixel format if necessary. It has to be freed after usage.
             * If the source processes frames on the GPU, it has to be downloaded to CPU memory before passing to callbacks.
             */
            CB_AVFRAME = 0b00000010
        };

        /*!
         * \private
         */
        virtual ~IFrameSource() = default;

        /*!
         * \brief Returns, whether the frame source is currently paused.
         * @return Paused state
         */
        virtual bool isPaused() = 0;


        /*!
         * \brief Register frame callback \c frameSink with given \c type
         * @param frameSink IFrameSink/IFrameFilter based object, its onFrame method with type \c type is called for every generated frame
         * @param type Callback type, can be linked with bitwise or to set multiple options
         * @return
         */
        Q_INVOKABLE virtual int registerCallback(IFrameSink *frameSink) = 0;

        /*!
         * \brief Removes frame callback \c frameSink from registry
         * @param frameSink Frame sink/filter to be removed
         * @return Previous position of the item, is -1 when not in registry
         */
        Q_INVOKABLE virtual int unregisterCallback(IFrameSink *frameSink) = 0;

    public slots:
        /*!
         * \brief Initialize frame source (e.g. open files, allocate buffers).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int init() = 0;

        /*!
         * \brief Clean up frame source (e.g. close network connections, free buffers).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int deinit() = 0;

        /*!
         * \brief Starts frame source (e.g. Start processing thread, activate camera).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int start() = 0;

        /*!
         * \brief Stops frame source (e.g. Interrupt processing thread, free camera).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int stop() = 0;

        /*!
         * \brief Sets paused flag of frame source
         *
         * If set to true, the frame source suspends providing any more frames until it is set to false again.
         * @param pause Paused flag
         * @return
         */
        Q_INVOKABLE virtual void pause(bool pause) = 0;

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
        * \brief Emitted when paused state changed
        * @param pause Current paused state
        */
        virtual void paused(bool pause) = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IFrameSource, "IFrameSource")

#endif //TRANSCODE_IFRAMESOURCE_H