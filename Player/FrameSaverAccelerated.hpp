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
// Created by silas on 18.12.21.
//

#ifndef LIBAVQT_FRAMESAVERACCELERATED_HPP
#define LIBAVQT_FRAMESAVERACCELERATED_HPP

#include <pgraph/impl/SimpleConsumer.hpp>
#include "AVQt/renderers/OpenGLFrameMapperFactory.hpp"
#include "AVQt/renderers/IOpenGLFrameMapper.hpp"
#include "pgraph_network/api/PadRegistry.hpp"

#include <QtOpenGL>

class FrameSaverAccelerated : public QObject, public pgraph::impl::SimpleConsumer, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit FrameSaverAccelerated(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry);
    ~FrameSaverAccelerated() override;

    void init();

    void consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) override;

protected slots:
    void onFrameReady(qint64 pts, const std::shared_ptr<QOpenGLFramebufferObject> &fbo);

private:
    std::shared_ptr<AVQt::api::IOpenGLFrameMapper> mapper;
    QOffscreenSurface *surface;
    QMutex contextMutex;
    QOpenGLContext *context;
    std::atomic_uint64_t frameCounter{0};
    int64_t outputPadId{pgraph::api::INVALID_PAD_ID};
};


#endif//LIBAVQT_FRAMESAVERACCELERATED_HPP
