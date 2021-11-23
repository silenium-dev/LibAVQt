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
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BELIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORTOR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OROTHER DEALINGS IN THE SOFTWARE.

//
// Created by silas on 23.11.21.
//

#ifndef LIBAVQT_IINPUT_HPP
#define LIBAVQT_IINPUT_HPP

#include <QtCore>

namespace AVQt {
    class IInput {
    public:
        virtual ~IInput() = default;
        virtual bool open() = 0;
        virtual void close() = 0;

        virtual bool start() = 0;
        virtual void stop() = 0;

        virtual void pause(bool state) = 0;
        virtual bool isPaused() = 0;

        virtual uint32_t getOutputPadId() const = 0;
    signals:
        virtual void started() = 0;
        virtual void stopped() = 0;
        virtual void paused(bool state) = 0;
    };
}// namespace AVQt

Q_DECLARE_INTERFACE(AVQt::IInput, "AVQt.IInput")

#endif//LIBAVQT_IINPUT_HPP
