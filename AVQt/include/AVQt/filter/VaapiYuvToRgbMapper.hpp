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

//
// Created by silas on 20.12.21.
//

#ifndef LIBAVQT_VAAPIYUVTORGBMAPPER_HPP
#define LIBAVQT_VAAPIYUVTORGBMAPPER_HPP

#include "AVQt/communication/IComponent.hpp"
#include "pgraph/api/Data.hpp"
#include "pgraph/impl/SimpleProcessor.hpp"
#include "pgraph_network/api/PadRegistry.hpp"
#include <QtCore>

namespace AVQt {
    class VaapiYuvToRgbMapperPrivate;
    class VaapiYuvToRgbMapper : public QThread, public api::IComponent, public pgraph::impl::SimpleProcessor {
        Q_OBJECT
        Q_DECLARE_PRIVATE(VaapiYuvToRgbMapper)
    public:
        Q_INVOKABLE explicit VaapiYuvToRgbMapper(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent = nullptr) Q_DECL_UNUSED;
        ~VaapiYuvToRgbMapper() Q_DECL_OVERRIDE;

        bool isOpen() const Q_DECL_OVERRIDE Q_DECL_UNUSED;
        bool isRunning() const Q_DECL_OVERRIDE Q_DECL_UNUSED;
        bool isPaused() const Q_DECL_OVERRIDE Q_DECL_UNUSED;

        void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) Q_DECL_OVERRIDE;

        bool init() Q_DECL_OVERRIDE;

    signals:
        void started() Q_DECL_OVERRIDE;
        void stopped() Q_DECL_OVERRIDE;
        void paused(bool state) Q_DECL_OVERRIDE;

    protected:
        bool open() Q_DECL_OVERRIDE;
        void close() Q_DECL_OVERRIDE;
        bool start() Q_DECL_OVERRIDE;
        void stop() Q_DECL_OVERRIDE;
        void pause(bool state) Q_DECL_OVERRIDE;

        void run() Q_DECL_OVERRIDE;

    private:
        std::unique_ptr<VaapiYuvToRgbMapperPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPIYUVTORGBMAPPER_HPP
