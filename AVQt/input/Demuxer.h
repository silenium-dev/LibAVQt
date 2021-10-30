#include "IPacketSource.h"

#include <ProcessingGraph/Producer.h>
#include <QIODevice>
#include <QThread>

extern "C" {
#include <libavcodec/packet.h>
}

#ifndef LIBAVQT_DEMUXER_H
#define LIBAVQT_DEMUXER_H

namespace AVQt {
    class DemuxerPrivate;

    class Demuxer : public QThread, public ProcessingGraph::Producer {
        Q_OBJECT
        //        Q_INTERFACES(AVQt::IPacketSource)

        Q_DECLARE_PRIVATE(AVQt::Demuxer)

    public:
        explicit Demuxer(QIODevice *inputDevice, QObject *parent = nullptr);

        explicit Demuxer(Demuxer &other) = delete;

        Demuxer &operator=(const Demuxer &other) = delete;

        ~Demuxer() override = default;

        //        Demuxer(Demuxer &&other) noexcept;

        /*!
         * \private
         */
        //        void run() override;

        /*!
         * \brief Returns, whether the frame source is currently paused.
         * @return Paused state
         */
        bool isPaused();

        /*!
         * \brief Register packet callback \c packetSink with given \c type
         * @param packetSink IPacketSink/IPacketFilter based object, its onPacket method with type \c type is called for every generated packet
         * @param type Callback type, can be linked with bitwise or to set multiple options
         * @return
         */
        //        Q_INVOKABLE qint64 registerCallback(IPacketSink *packetSink, int8_t type);

        /*!
         * \brief Removes packet callback \c packetSink from registry
         * @param packetSink Packet sink/filter to be removed
         * @return Previous position of the item, is -1 when not in registry
         */
        //        Q_INVOKABLE qint64 unregisterCallback(IPacketSink *packetSink);

    public slots:
        /*!
         * \brief Initialize packet source (e.g. open files, allocate buffers).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int init();

        /*!
         * \brief Clean up packet source (e.g. close network connections, free buffers).
         * @return Status code (0 = Success)
         */
        Q_INVOKABLE int deinit();

        /*!
         * \brief Starts packet source (e.g. Start processing thread, activate camera).
         * @return Status code (0 = Success)
         */
        //        Q_INVOKABLE int start();

        /*!
         * \brief Stops packet source (e.g. Interrupt processing thread, free camera).
         * @return Status code (0 = Success)
         */
        //        Q_INVOKABLE int stop();

        /*!
         * \brief Sets paused flag of packet source
         *
         * If set to true, the packet source suspends providing any more packets until it is set to false again.
         * @param pause Paused flag
         * @return
         */
        Q_INVOKABLE void pause(bool pause);

    signals:

        /*!
         * \brief Emitted when started
         */
        void started();

        /*!
         * \brief Emitted when stopped
         */
        void stopped();

        /*!
         * \brief Emitted when paused state changed
         * @param pause Current paused state
         */
        void paused(bool pause);

    protected:
        [[maybe_unused]] explicit Demuxer(DemuxerPrivate &p);

        DemuxerPrivate *d_ptr;
    };
}// namespace AVQt

PG_DECLARE_DATATYPE(AVPacket *)

#endif//LIBAVQT_DEMUXER_H
