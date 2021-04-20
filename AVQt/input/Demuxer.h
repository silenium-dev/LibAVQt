//
// Created by silas on 3/25/21.
//

#include "IPacketSource.h"

#include <QThread>

#ifndef LIBAVQT_DEMUXER_H
#define LIBAVQT_DEMUXER_H

namespace AVQt {
    class DemuxerPrivate;

    class Demuxer : public QThread, public IPacketSource {
    Q_OBJECT
        Q_INTERFACES(AVQt::IPacketSource)

        Q_DECLARE_PRIVATE(AVQt::Demuxer)

    public:
        [[maybe_unused]] explicit Demuxer(QIODevice *inputDevice, QObject *parent = nullptr);

        explicit Demuxer(Demuxer &other) = delete;

        explicit Demuxer(Demuxer &&other);

        /*!
         * \private
         */
        void run() Q_DECL_OVERRIDE;

        /*!
         * \brief Returns, whether the frame source is currently paused.
         * @return Paused state
         */
        bool isPaused() Q_DECL_OVERRIDE;

        /*!
         * \brief Register packet callback \c packetSink with given \c type
         * @param packetSink IPacketSink/IPacketFilter based object, its onPacket method with type \c type is called for every generated packet
         * @param type Callback type, can be linked with bitwise or to set multiple options
         * @return
         */
        Q_INVOKABLE qsizetype registerCallback(IPacketSink *packetSink, uint8_t type) Q_DECL_OVERRIDE;

        /*!
         * \brief Removes packet callback \c packetSink from registry
         * @param packetSink Packet sink/filter to be removed
         * @return Previous position of the item, is -1 when not in registry
         */
        Q_INVOKABLE qsizetype unregisterCallback(IPacketSink *packetSink) Q_DECL_OVERRIDE;

        Demuxer &operator=(const Demuxer &other) = delete;

        Demuxer &operator=(Demuxer &&other) noexcept;

    public slots:
        /*!
         * \brief Initialize packet source (e.g. open files, allocate buffers).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int init() Q_DECL_OVERRIDE;

        /*!
         * \brief Clean up packet source (e.g. close network connections, free buffers).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int deinit() Q_DECL_OVERRIDE;

        /*!
         * \brief Starts packet source (e.g. Start processing thread, activate camera).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int start() Q_DECL_OVERRIDE;

        /*!
         * \brief Stops packet source (e.g. Interrupt processing thread, free camera).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int stop();

        /*!
         * \brief Sets paused flag of packet source
         *
         * If set to true, the packet source suspends providing any more packets until it is set to false again.
         * @param pause Paused flag
         * @return
         */
        Q_INVOKABLE void pause(bool pause) Q_DECL_OVERRIDE;

    signals:

        /*!
         * \brief Emitted when started
         */
        void started() Q_DECL_OVERRIDE;

        /*!
         * \brief Emitted when stopped
         */
        void stopped() Q_DECL_OVERRIDE;

        /*!
         * \brief Emitted when paused state changed
         * @param pause Current paused state
         */
        void paused(bool pause) Q_DECL_OVERRIDE;

    protected:
        explicit Demuxer(DemuxerPrivate &p);

        DemuxerPrivate *d_ptr;
    };
}

#endif //LIBAVQT_DEMUXER_H