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

//
// Created by silas on 20.12.21.
//

#ifndef LIBAVQT_VAAPIYUVTORGBMAPPER_HPP
#define LIBAVQT_VAAPIYUVTORGBMAPPER_HPP

#include "communication/IComponent.hpp"
#include <QtCore>
#include "pgraph/api/Data.hpp"
#include "pgraph/impl/SimpleProcessor.hpp"
#include "pgraph_network/api/PadRegistry.hpp"

namespace AVQt {
    class VaapiYuvToRgbMapperPrivate;
    class VaapiYuvToRgbMapper : public QThread, public api::IComponent, public pgraph::impl::SimpleProcessor {
        Q_OBJECT
        Q_DECLARE_PRIVATE(VaapiYuvToRgbMapper)
    public:
        Q_INVOKABLE VaapiYuvToRgbMapper(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr);
        ~VaapiYuvToRgbMapper() override;

        bool isOpen() const override;
        bool isRunning() const override;
        bool isPaused() const override;

        bool open() override;
        void close() override;
        bool init() override;
        bool start() override;
        void stop() override;
        void pause(bool state) override;

        void consume(uint32_t pad, std::shared_ptr<pgraph::api::Data> data) override;

    signals:
        void started() override;
        void stopped() override;
        void paused(bool state) override;

    protected:
        void run() override;

    private:
        VaapiYuvToRgbMapperPrivate *d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPIYUVTORGBMAPPER_HPP
