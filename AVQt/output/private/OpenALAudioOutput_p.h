//
// Created by silas on 01.09.21.
//

#ifndef LIBAVQT_OPENALAUDIOOUTPUT_P_H
#define LIBAVQT_OPENALAUDIOOUTPUT_P_H

#include "../OpenALAudioOutput.h"

#include <QtCore>

namespace AVQt {
    class OpenALAudioOutputPrivate : public QObject {
    Q_OBJECT

        Q_DECLARE_PUBLIC(AVQt::OpenALAudioOutput)

    private:
        explicit OpenALAudioOutputPrivate(OpenALAudioOutput *q) : q_ptr(q) {}

        OpenALAudioOutput *q_ptr;

    };
}

#endif //LIBAVQT_OPENALAUDIOOUTPUT_P_H