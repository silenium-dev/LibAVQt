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

#ifndef LIBAVQT_VAAPIYUVTORGBMAPPER_P_HPP
#define LIBAVQT_VAAPIYUVTORGBMAPPER_P_HPP

#include <QtCore>
#include "communication/VideoPadParams.hpp"

extern "C" {
#include <libavfilter/avfilter.h>
}

namespace AVQt {
    class VaapiYuvToRgbMapper;
    class VaapiYuvToRgbMapperPrivate {
        Q_DECLARE_PUBLIC(VaapiYuvToRgbMapper)
    private:
        explicit VaapiYuvToRgbMapperPrivate(VaapiYuvToRgbMapper *q) : q_ptr(q) {}
        VaapiYuvToRgbMapper *q_ptr;

        std::atomic<AVBufferRef *> pHWDeviceCtx{nullptr};
        std::atomic<AVBufferRef *> pInputHWFramesCtx{nullptr}, pOutputHWFramesCtx{nullptr};

        AVFrame *currentFrame{nullptr}, *pHWFrame{nullptr};
        int64_t frameCounter{0};
        VideoPadParams videoParams;

        AVFilterContext *pBufferSrcCtx{nullptr};
        AVFilterContext *pBufferSinkCtx{nullptr};
        AVFilterContext *pVAAPIScaleCtx{nullptr};
        AVFilterContext *pVAAPIDenoiseCtx{nullptr};
        AVFilterGraph *pFilterGraph{nullptr};
        AVFilterInOut *pInputs{nullptr};
        AVFilterInOut *pOutputs{nullptr};

        QMutex inputQueueMutex;
        QQueue<AVFrame *> inputQueue;

        std::atomic_bool initialized{false}, running{false}, paused{false}, open{false}, pipelineInitialized{false};

        int64_t inputPadId;
        int64_t outputPadId;
        std::shared_ptr<VideoPadParams> outputPadParams;

        static constexpr size_t maxInputQueueSize{6};

        friend class VaapiYuvToRgbMapper;
    };
}// namespace AVQt


#endif//LIBAVQT_VAAPIYUVTORGBMAPPER_P_HPP
