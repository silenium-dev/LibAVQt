//
// Created by silas on 3/25/21.
//

#include <QObject>

#ifndef LIBAVQT_IPACKETSOURCE_H
#define LIBAVQT_IPACKETSOURCE_H

namespace AVQt {
    class IPacketSink;

    class IPacketSource {
    public:
        /*!
         * \enum CB_TYPE
         * \brief Used to define accepted callback types of a packet sink.
         *
         * Can be linked with bitwise-or to set multiple types
         */
        enum CB_TYPE : int8_t {
            /*!
             * \private
             */
            CB_NONE = -1,

            /*!
             * \brief Callback type: Audio packet, if an audio packet is read, it will be passed to all registered audio packet sinks
             */
            CB_AUDIO = 0b00000001,

            /*!
             * \brief Callback type: Video packet
             */
            CB_VIDEO = 0b00000010,

            /*!
             * \brief Callback type: Subtitle packet
             */
            CB_SUBTITLE = 0b00000100
        };

        /*!
         * \private
         */
        virtual ~IPacketSource() = default;

        /*!
         * \brief Returns, whether the frame source is currently paused.
         * @return Paused state
         */
        virtual bool isPaused() = 0;

        /*!
         * \brief Register packet callback \c packetSink with given \c type
         * @param packetSink IPacketSink/IPacketFilter based object, its onPacket method with type \c type is called for every generated packet
         * @param type Callback type, can be linked with bitwise or to set multiple options
         * @return
         */
        Q_INVOKABLE virtual int registerCallback(IPacketSink *packetSink, uint8_t type) = 0;

        /*!
         * \brief Removes packet callback \c packetSink from registry
         * @param packetSink Packet sink/filter to be removed
         * @return Previous position of the item, is -1 when not in registry
         */
        Q_INVOKABLE virtual int unregisterCallback(IPacketSink *packetSink) = 0;

    public slots:
        /*!
         * \brief Initialize packet source (e.g. open files, allocate buffers).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int init() = 0;

        /*!
         * \brief Clean up packet source (e.g. close network connections, free buffers).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int deinit() = 0;

        /*!
         * \brief Starts packet source (e.g. Start processing thread, activate camera).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int start() = 0;

        /*!
         * \brief Stops packet source (e.g. Interrupt processing thread, free camera).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE virtual int stop() = 0;

        /*!
         * \brief Sets paused flag of packet source
         *
         * If set to true, the packet source suspends providing any more packets until it is set to false again.
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

Q_DECLARE_INTERFACE(AVQt::IPacketSource, "AVQt::IPacketSource")

#endif //LIBAVQT_IPACKETSOURCE_H
