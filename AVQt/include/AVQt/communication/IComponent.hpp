// Copyright (c) 2021-2022.
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


#ifndef LIBAVQT_ICOMPONENT_HPP
#define LIBAVQT_ICOMPONENT_HPP

#include <QtCore/QObject>

namespace AVQt::api {
    class IComponent {
    public:
        virtual ~IComponent() = default;

        [[nodiscard]] virtual bool isOpen() const = 0;
        [[nodiscard]] virtual bool isRunning() const = 0;
        [[nodiscard]] virtual bool isPaused() const = 0;

        virtual bool init() = 0;

    protected:
        virtual bool open() = 0;
        virtual void close() = 0;

        virtual bool start() = 0;
        virtual void stop() = 0;

        virtual void pause(bool state) = 0;
    signals:
        virtual void started() = 0;
        virtual void stopped() = 0;
        virtual void paused(bool state) = 0;
    };
}// namespace AVQt::api

Q_DECLARE_INTERFACE(AVQt::api::IComponent, "AVQt.api.IComponent")

#endif//LIBAVQT_ICOMPONENT_HPP
