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

#include "include/AVQt/filter/VaapiYuvToRgbMapper.hpp"
#include "global.hpp"
#include "communication/Message.hpp"
#include "communication/VideoPadParams.hpp"
#include "private/VaapiYuvToRgbMapper_p.hpp"

#include <pgraph_network/impl/RegisteringPadFactory.hpp>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>
}

namespace AVQt {
    VaapiYuvToRgbMapper::VaapiYuvToRgbMapper(std::shared_ptr<pgraph::network::api::PadRegistry> padRegistry, QObject *parent)
        : QThread(parent),
          pgraph::impl::SimpleProcessor(pgraph::network::impl::RegisteringPadFactory::factoryFor(padRegistry)),
          d_ptr(new VaapiYuvToRgbMapperPrivate(this)) {
    }

    VaapiYuvToRgbMapper::~VaapiYuvToRgbMapper() {
        VaapiYuvToRgbMapper::close();
        delete d_ptr;
    }

    void VaapiYuvToRgbMapper::consume(uint32_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(VaapiYuvToRgbMapper);
        if (data->getType() == Message::Type) {
            auto message = std::dynamic_pointer_cast<Message>(data);
            switch (static_cast<AVQt::Message::Action::Enum>(message->getAction())) {

                case Message::Action::INIT:
                    if (message->getPayloads().contains("videoParams")) {
                        d->videoParams = message->getPayloads().value("videoParams").value<VideoPadParams>();
                    }
                    if (!open()) {
                        qWarning() << "Failed to open VaapiYuvToRgbMapper";
                        return;
                    }
                    break;
                case Message::Action::CLEANUP:
                    close();
                    break;
                case Message::Action::START:
                    if (!start()) {
                        qWarning() << "Failed to start VaapiYuvToRgbMapper";
                    }
                    break;
                case Message::Action::STOP:
                    stop();
                    break;
                case Message::Action::PAUSE:
                    if (!message->getPayloads().contains("state")) {
                        qWarning() << "No pause value in message";
                        return;
                    }
                    pause(message->getPayload("state").toBool());
                    break;
                case Message::Action::DATA:
                    if (message->getPayloads().contains("frame")) {
                        auto frame = message->getPayload("frame").value<AVFrame *>();
                        if (frame && frame->format == AV_PIX_FMT_VAAPI) {
                            QMutexLocker lock(&d->inputQueueMutex);
                            while (d->inputQueue.size() >= VaapiYuvToRgbMapperPrivate::maxInputQueueSize) {
                                lock.unlock();
                                QThread::msleep(2);
                                lock.relock();
                            }
                            d->inputQueue.enqueue(av_frame_clone(frame));
                        } else {
                            qWarning() << "Received frame with wrong format";
                            av_frame_free(&frame);
                        }
                    }
                    break;
                case Message::Action::NONE:
                    qWarning() << "Received message with no action";
                    break;
            }
        }
    }
    bool VaapiYuvToRgbMapper::open() {
        Q_D(VaapiYuvToRgbMapper);

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            d->pFilterGraph = avfilter_graph_alloc();
            avfilter_graph_parse2(d->pFilterGraph, "scale_vaapi=format=bgra", &d->pInputs, &d->pOutputs);
            d->outputPadParams->frameSize = d->videoParams.frameSize;
            d->outputPadParams->swPixelFormat = AV_PIX_FMT_BGRA;
            d->outputPadParams->pixelFormat = AV_PIX_FMT_VAAPI;
            d->outputPadParams->isHWAccel = true;
            return true;
        } else {
            qWarning("Already initialized");
            return false;
        }
    }
    void VaapiYuvToRgbMapper::close() {
        Q_D(VaapiYuvToRgbMapper);

        if (!d->initialized) {
            qWarning("Not initialized");
            return;
        }

        stop();

        auto outputHWFramesCtx = d->pOutputHWFramesCtx.load();
        av_buffer_unref(&outputHWFramesCtx);
        d->pOutputHWFramesCtx.store(nullptr);
        auto inputHWFramesCtx = d->pInputHWFramesCtx.load();
        av_buffer_unref(&inputHWFramesCtx);
        d->pInputHWFramesCtx.store(nullptr);
        auto hwDeviceCtx = d->pHWDeviceCtx.load();
        av_buffer_unref(&hwDeviceCtx);
        d->pHWDeviceCtx.store(nullptr);

        avfilter_free(d->pBufferSrcCtx);
        avfilter_free(d->pBufferSinkCtx);
        avfilter_free(d->pVAAPIScaleCtx);
        avfilter_inout_free(&d->pInputs);
        avfilter_inout_free(&d->pOutputs);
        avfilter_graph_free(&d->pFilterGraph);
    }
    bool VaapiYuvToRgbMapper::isOpen() const {
        Q_D(const VaapiYuvToRgbMapper);
        return d->initialized;
    }
    bool VaapiYuvToRgbMapper::isRunning() const {
        Q_D(const VaapiYuvToRgbMapper);
        return d->running;
    }
    bool VaapiYuvToRgbMapper::init() {
        Q_D(VaapiYuvToRgbMapper);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            d->outputPadParams = std::make_shared<VideoPadParams>();
            d->outputPadId = createOutputPad(d->outputPadParams);
            if (d->outputPadId < 0) {
                qWarning("Failed to create output pad");
                return false;
            }
            d->inputPadId = createInputPad(pgraph::api::PadUserData::emptyUserData());
            return true;
        }
        qWarning("Already initialized");
        return false;
    }
    bool VaapiYuvToRgbMapper::start() {
        Q_D(VaapiYuvToRgbMapper);

        if (!d->initialized) {
            qWarning("Not initialized");
            return false;
        }

        bool shouldBe = false;
        if (d->running.compare_exchange_strong(shouldBe, true)) {
            d->paused = false;
            QThread::start();
            return true;
        } else {
            qWarning("Already running");
        }
        return false;
    }
    void VaapiYuvToRgbMapper::stop() {
        Q_D(VaapiYuvToRgbMapper);

        if (!d->initialized) {
            qWarning("Not initialized");
            return;
        }

        bool shouldBe = true;
        if (d->running.compare_exchange_strong(shouldBe, false)) {
            d->paused = true;
            QThread::wait();
            {
                QMutexLocker lock(&d->inputQueueMutex);
                while (!d->inputQueue.isEmpty()) {
                    auto frame = d->inputQueue.dequeue();
                    av_frame_free(&frame);
                }
            }
        } else {
            qWarning("Not running");
        }
    }
    void VaapiYuvToRgbMapper::pause(bool state) {
        Q_D(VaapiYuvToRgbMapper);

        if (!d->initialized) {
            qWarning("Not initialized");
            return;
        }

        bool shouldBe = !state;
        if (d->paused.compare_exchange_strong(shouldBe, state)) {
            paused(state);
        } else {
            qWarning("Already %s", state ? "paused" : "running");
        }
    }

    bool VaapiYuvToRgbMapper::isPaused() const {
        Q_D(const VaapiYuvToRgbMapper);
        return d->paused;
    }

    void VaapiYuvToRgbMapper::run() {
        Q_D(VaapiYuvToRgbMapper);

        if (!d->initialized) {
            qWarning("Not initialized");
            return;
        }

        if (!d->running) {
            qWarning("Not running");
            return;
        }

        if (d->paused) {
            qWarning("Paused");
            return;
        }

        constexpr auto strBufSize = 64;
        char strBuf[strBufSize];
        int ret;

        while (d->running) {
            QMutexLocker lock(&d->inputQueueMutex);
            if (d->paused || d->inputQueue.isEmpty()) {
                lock.unlock();
                msleep(1);
            } else {
                if (d->currentFrame) {
                    av_frame_free(&d->currentFrame);
                }
                ++d->frameCounter;
                auto entry = d->inputQueue.dequeue();
                lock.unlock();

                d->currentFrame = entry;

                bool shouldBe = false;
                if (d->pipelineInitialized.compare_exchange_strong(shouldBe, true)) {
                    if (!d->pInputHWFramesCtx.load()) {
                        if (d->currentFrame->hw_frames_ctx) {
                            d->pHWDeviceCtx = av_buffer_ref(reinterpret_cast<AVHWFramesContext *>(d->currentFrame->hw_frames_ctx->data)->device_ref);
                            d->pInputHWFramesCtx = av_buffer_ref(d->currentFrame->hw_frames_ctx);
                        } else {
                            AVBufferRef *pDeviceCtx;
                            if (0 != av_hwdevice_ctx_create(&pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0)) {
                                qFatal("Could not create AVHWDeviceContext");
                            }
                            d->pHWDeviceCtx.store(pDeviceCtx);
                            AVBufferRef *pFramesCtx;
                            pFramesCtx = av_hwframe_ctx_alloc(d->pHWDeviceCtx.load());
                            if (!pFramesCtx) {
                                qFatal("Could not allocate frames context");
                            }
                            auto framesContext = reinterpret_cast<AVHWFramesContext *>(pFramesCtx->data);
                            framesContext->width = d->currentFrame->width;
                            framesContext->height = d->currentFrame->height;
                            framesContext->format = AV_PIX_FMT_VAAPI;
                            framesContext->sw_format = static_cast<AVPixelFormat>(d->currentFrame->format);
                            framesContext->initial_pool_size = 64;
                            ret = av_hwframe_ctx_init(pFramesCtx);
                            if (0 != ret) {
                                qFatal("%d: Could not initialize input AVHWFramesContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                            }
                            d->pInputHWFramesCtx.store(pFramesCtx);
                        }
                    }

                    auto pFramesCtx = av_hwframe_ctx_alloc(d->pHWDeviceCtx);
                    auto framesContext = reinterpret_cast<AVHWFramesContext *>(pFramesCtx->data);
                    framesContext->width = d->currentFrame->width;
                    framesContext->height = d->currentFrame->height;
                    framesContext->format = AV_PIX_FMT_VAAPI;
                    framesContext->sw_format = AV_PIX_FMT_BGRA;
                    ret = av_hwframe_ctx_init(pFramesCtx);
                    if (0 != ret) {
                        qFatal("%d: Could not initialize output AVHWFramesContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                    d->pOutputHWFramesCtx.store(pFramesCtx);

                    d->pHWFrame = av_frame_alloc();
                    d->pHWFrame->format = AV_PIX_FMT_VAAPI;
                    ret = av_hwframe_get_buffer(d->pOutputHWFramesCtx.load(), d->pHWFrame, 0);
                    if (ret != 0) {
                        qFatal("%d: Could not get buffer for hw frame: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }

                    for (auto i = 0; i < d->pFilterGraph->nb_filters; ++i) {
                        d->pFilterGraph->filters[i]->hw_device_ctx = av_buffer_ref(d->pHWDeviceCtx.load());
                    }

                    char args[512];
                    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                             d->currentFrame->width, d->currentFrame->height, AV_PIX_FMT_VAAPI,
                             1, 1000000,
                             1, 1);

                    ret = avfilter_graph_create_filter(&d->pBufferSrcCtx, avfilter_get_by_name("buffer"), "in",
                                                       args, nullptr, d->pFilterGraph);
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }
                    //            ret = avfilter_graph_create_filter(&d->pVAAPIDenoiseCtx, avfilter_get_by_name("denoise_vaapi"), "vaapi_denoise",
                    //                                               nullptr, nullptr, d->pFilterGraph);
                    //            if (ret != 0) {
                    //                qFatal("Could not link filters: %d", ret);
                    //            }
                    ret = avfilter_graph_create_filter(&d->pVAAPIScaleCtx, avfilter_get_by_name("scale_vaapi"), "vaapi_scale",
                                                       "format=bgra", nullptr, d->pFilterGraph);
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }

                    ret = avfilter_graph_create_filter(&d->pBufferSinkCtx, avfilter_get_by_name("buffersink"), "out",
                                                       nullptr, nullptr, d->pFilterGraph);
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }

                    ret = avfilter_link(d->pBufferSrcCtx, 0, d->pInputs->filter_ctx, d->pInputs->pad_idx);
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }
                    ret = avfilter_link(d->pOutputs->filter_ctx, d->pOutputs->pad_idx, d->pVAAPIScaleCtx, 0);
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }
                    //            ret = avfilter_link(d->pVAAPIScaleCtx, 0, d->pVAAPIDenoiseCtx, 0);
                    //            if (ret != 0) {
                    //                qFatal("Could not link filters: %d", ret);
                    //            }
                    ret = avfilter_link(d->pVAAPIScaleCtx, 0, d->pBufferSinkCtx, 0);
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }

                    d->pBufferSrcCtx->outputs[0]->hw_frames_ctx = av_buffer_ref(d->pInputHWFramesCtx.load());
                    //            d->pVAAPIDenoiseCtx->inputs[0]->hw_frames_ctx = av_buffer_ref(d->pInputHWFramesCtx);
                    //            d->pVAAPIDenoiseCtx->outputs[0]->hw_frames_ctx = av_buffer_ref(d->pInputHWFramesCtx);
                    d->pVAAPIScaleCtx->inputs[0]->hw_frames_ctx = av_buffer_ref(d->pInputHWFramesCtx.load());
                    d->pVAAPIScaleCtx->outputs[0]->hw_frames_ctx = av_buffer_ref(d->pOutputHWFramesCtx.load());
                    d->pBufferSinkCtx->inputs[0]->hw_frames_ctx = av_buffer_ref(d->pOutputHWFramesCtx.load());

                    ret = avfilter_graph_config(d->pFilterGraph, nullptr);
                    if (ret != 0) {
                        qFatal("%d: Could not configure filter graph: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                }

                ret = av_buffersrc_add_frame(d->pBufferSrcCtx, d->currentFrame);
                if (ret != 0) {
                    qFatal("%d: Could not enqueue frame into filter graph: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }
                ret = av_buffersink_get_frame(d->pBufferSinkCtx, d->pHWFrame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    continue;
                } else if (ret != 0) {
                    qFatal("%d: Could not get frame from filter graph: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }

                produce(Message::builder().withAction(Message::Action::DATA).withPayload("frame", QVariant::fromValue(d->pHWFrame)).build(), d->outputPadId);

                av_frame_unref(d->pHWFrame);
            }
        }
    }
}// namespace AVQt
