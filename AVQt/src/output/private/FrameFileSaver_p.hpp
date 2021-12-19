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

/*!
 * \private
 * \internal
 */
#include "output/FrameFileSaver.hpp"

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
