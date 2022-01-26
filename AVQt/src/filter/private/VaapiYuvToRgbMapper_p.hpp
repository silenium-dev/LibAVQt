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

#ifndef LIBAVQT_VAAPIYUVTORGBMAPPER_P_HPP
#define LIBAVQT_VAAPIYUVTORGBMAPPER_P_HPP

#include "communication/VideoPadParams.hpp"
#include <QtCore>

extern "C" {
#include <libavfilter/avfilter.h>
}

namespace AVQt {
    class VaapiYuvToRgbMapper;
    class VaapiYuvToRgbMapperPrivate {
        Q_DECLARE_PUBLIC(VaapiYuvToRgbMapper)
    public:
        static void destroyAVBufferRef(AVBufferRef *ref);
        static void destroyAVFilterContext(AVFilterContext *filterContext);
        static void destroyAVFilterGraph(AVFilterGraph *filterGraph);
        static void destroyAVFilterInOut(AVFilterInOut *filterInOut);

    private:
        explicit VaapiYuvToRgbMapperPrivate(VaapiYuvToRgbMapper *q) : q_ptr(q) {}
        VaapiYuvToRgbMapper *q_ptr;

        std::unique_ptr<AVBufferRef, decltype(&destroyAVBufferRef)> pHWDeviceCtx{nullptr, &destroyAVBufferRef};
        std::unique_ptr<AVBufferRef, decltype(&destroyAVBufferRef)> pInputHWFramesCtx{nullptr, &destroyAVBufferRef}, pOutputHWFramesCtx{nullptr, &destroyAVBufferRef};

        std::shared_ptr<AVFrame> currentFrame{};
        int64_t frameCounter{0};
        communication::VideoPadParams videoParams;

        std::unique_ptr<AVFilterContext, decltype(&destroyAVFilterContext)> pBufferSrcCtx{nullptr, &destroyAVFilterContext};
        std::unique_ptr<AVFilterContext, decltype(&destroyAVFilterContext)> pBufferSinkCtx{nullptr, &destroyAVFilterContext};
        std::unique_ptr<AVFilterContext, decltype(&destroyAVFilterContext)> pVAAPIScaleCtx{nullptr, &destroyAVFilterContext};
        std::unique_ptr<AVFilterContext, decltype(&destroyAVFilterContext)> pVAAPIDenoiseCtx{nullptr, &destroyAVFilterContext};
        std::unique_ptr<AVFilterGraph, decltype(&destroyAVFilterGraph)> pFilterGraph{nullptr, &destroyAVFilterGraph};
        std::unique_ptr<AVFilterInOut, decltype(&destroyAVFilterInOut)> pInputs{nullptr, &destroyAVFilterInOut}, pOutputs{nullptr, &destroyAVFilterInOut};

        QMutex inputQueueMutex;
        QQueue<std::shared_ptr<AVFrame>> inputQueue;

        std::atomic_bool initialized{false}, running{false}, paused{false}, open{false}, pipelineInitialized{false};

        int64_t inputPadId;
        int64_t outputPadId;
        std::shared_ptr<communication::VideoPadParams> outputPadParams;

        static constexpr size_t maxInputQueueSize{6};

        friend class VaapiYuvToRgbMapper;
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPIYUVTORGBMAPPER_P_HPP
