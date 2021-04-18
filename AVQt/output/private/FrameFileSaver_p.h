/*!
 * \private
 * \internal
 */
#include "../FrameFileSaver.h"

#include <QObject>

#ifndef TRANSCODE_FRAMESAVER_P_H
#define TRANSCODE_FRAMESAVER_P_H

namespace AVQt {
    /*!
     * \private
     */
    struct FrameFileSaverPrivate {
        explicit FrameFileSaverPrivate(FrameFileSaver *q) : q_ptr(q) {}

        FrameFileSaver *q_ptr;

        std::atomic_uint64_t m_frameNumber {0};
        std::atomic_bool m_isPaused  {false};
        quint64 m_frameInterval = 1;
        QString m_filePrefix{""};
    };
}

#endif //TRANSCODE_FRAMESAVER_P_H