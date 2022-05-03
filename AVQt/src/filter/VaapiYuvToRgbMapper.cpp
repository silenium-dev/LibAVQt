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


#include "include/AVQt/filter/VaapiYuvToRgbMapper.hpp"
#include "AVQt/communication/Message.hpp"
#include "AVQt/communication/VideoPadParams.hpp"
#include "global.hpp"
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
        Q_D(VaapiYuvToRgbMapper);
        if (d->open) {
            VaapiYuvToRgbMapper::close();
        }
    }

    void VaapiYuvToRgbMapper::consume(int64_t pad, std::shared_ptr<pgraph::api::Data> data) {
        Q_D(VaapiYuvToRgbMapper);
        if (data->getType() == communication::Message::Type) {
            auto message = std::dynamic_pointer_cast<communication::Message>(data);
            switch (static_cast<AVQt::communication::Message::Action::Enum>(message->getAction())) {

                case communication::Message::Action::INIT:
                    if (message->getPayloads().contains("videoParams")) {
                        d->videoParams = message->getPayloads().value("videoParams").value<communication::VideoPadParams>();
                    }
                    if (!open()) {
                        qWarning() << "Failed to open VaapiYuvToRgbMapper";
                        return;
                    }
                    break;
                case communication::Message::Action::CLEANUP:
                    close();
                    break;
                case communication::Message::Action::START:
                    if (!start()) {
                        qWarning() << "Failed to start VaapiYuvToRgbMapper";
                    }
                    break;
                case communication::Message::Action::STOP:
                    stop();
                    break;
                case communication::Message::Action::PAUSE:
                    if (!message->getPayloads().contains("state")) {
                        qWarning() << "No pause value in message";
                        return;
                    }
                    pause(message->getPayload("state").toBool());
                    break;
                case communication::Message::Action::DATA:
                    if (message->getPayloads().contains("frame")) {
                        auto frame = message->getPayload("frame").value<std::shared_ptr<AVFrame>>();
                        if (frame && frame->format == AV_PIX_FMT_VAAPI) {
                            QMutexLocker lock(&d->inputQueueMutex);
                            while (d->inputQueue.size() >= VaapiYuvToRgbMapperPrivate::maxInputQueueSize) {
                                lock.unlock();
                                QThread::msleep(2);
                                lock.relock();
                            }
                            d->inputQueue.enqueue(frame);
                        } else {
                            qWarning() << "Received frame with wrong format";
                        }
                    }
                    break;
                case communication::Message::Action::NONE:
                    qWarning() << "Received message with no action";
                    break;
            }
        }
    }
    bool VaapiYuvToRgbMapper::open() {
        Q_D(VaapiYuvToRgbMapper);

        if (!d->initialized) {
            qWarning("Not initialized");
            return false;
        }

        bool shouldBe = false;
        if (d->open.compare_exchange_strong(shouldBe, true)) {
            d->pFilterGraph.reset(avfilter_graph_alloc());
            AVFilterInOut *inputs = nullptr, *outputs = nullptr;
            avfilter_graph_parse2(d->pFilterGraph.get(), "scale_vaapi=format=bgra", &inputs, &outputs);
            if (!inputs || !outputs) {
                qWarning() << "Failed to create filter graph";
                return false;
            }
            d->pInputs.reset(inputs);
            d->pOutputs.reset(outputs);

            d->outputPadParams->frameSize = d->videoParams.frameSize;
            d->outputPadParams->swPixelFormat = AV_PIX_FMT_BGRA;
            d->outputPadParams->pixelFormat = AV_PIX_FMT_VAAPI;
            d->outputPadParams->isHWAccel = true;
            produce(communication::Message::builder().withAction(communication::Message::Action::INIT).withPayload("videoParams", QVariant::fromValue(*d->outputPadParams)).build(), d->outputPadId);
            return true;
        } else {
            qWarning("Already initialized");
            return false;
        }
    }
    void VaapiYuvToRgbMapper::close() {
        Q_D(VaapiYuvToRgbMapper);

        if (d->running) {
            stop();
        }
        bool shouldBe = true;
        if (d->open.compare_exchange_strong(shouldBe, false)) {
            d->pOutputHWFramesCtx.reset();
            d->pInputHWFramesCtx.reset();
            d->pHWDeviceCtx.reset();

            d->pInputs.reset();
            d->pOutputs.reset();
            d->pBufferSrcCtx.reset();
            d->pVAAPIDenoiseCtx.reset();
            d->pVAAPIScaleCtx.reset();
            d->pBufferSinkCtx.reset();
            d->pFilterGraph.reset();

            produce(communication::Message::builder().withAction(communication::Message::Action::CLEANUP).build(), d->outputPadId);
        }
    }
    bool VaapiYuvToRgbMapper::isOpen() const {
        Q_D(const VaapiYuvToRgbMapper);
        return d->open;
    }
    bool VaapiYuvToRgbMapper::isRunning() const {
        Q_D(const VaapiYuvToRgbMapper);
        return d->running;
    }
    bool VaapiYuvToRgbMapper::init() {
        Q_D(VaapiYuvToRgbMapper);
        bool shouldBe = false;
        if (d->initialized.compare_exchange_strong(shouldBe, true)) {
            d->outputPadParams = std::make_shared<communication::VideoPadParams>();
            d->outputPadId = createOutputPad(d->outputPadParams);
            if (d->outputPadId < 0) {
                qWarning("Failed to create output pad");
                return false;
            }
            d->inputPadId = createInputPad(pgraph::api::PadUserData::emptyUserData());
            if (d->inputPadId == pgraph::api::INVALID_PAD_ID) {
                qWarning() << "Failed to create input pad";
                return false;
            }
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

            produce(communication::Message::builder().withAction(communication::Message::Action::START).build(), d->outputPadId);

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
            produce(communication::Message::builder().withAction(communication::Message::Action::STOP).build(), d->outputPadId);

            QThread::wait();
            {
                QMutexLocker lock(&d->inputQueueMutex);
                d->inputQueue.clear();
            }
            d->paused = true;
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

        if (!d->open) {
            qWarning("Not open");
            return;
        }

        if (!d->running) {
            qWarning("Not running");
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
                ++d->frameCounter;
                auto entry = d->inputQueue.dequeue();
                lock.unlock();

                d->currentFrame = std::move(entry);

                bool shouldBe = false;
                if (d->pipelineInitialized.compare_exchange_strong(shouldBe, true)) {
                    if (!d->pInputHWFramesCtx) {
                        if (d->currentFrame->hw_frames_ctx) {
                            d->pHWDeviceCtx.reset(av_buffer_ref(reinterpret_cast<AVHWFramesContext *>(d->currentFrame->hw_frames_ctx->data)->device_ref));
                            d->pInputHWFramesCtx.reset(av_buffer_ref(d->currentFrame->hw_frames_ctx));
                        } else {
                            AVBufferRef *pDeviceCtx;
                            if (0 != av_hwdevice_ctx_create(&pDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0)) {
                                qFatal("Could not create AVHWDeviceContext");
                            }
                            d->pHWDeviceCtx.reset(pDeviceCtx);
                            AVBufferRef *pFramesCtx;
                            pFramesCtx = av_hwframe_ctx_alloc(d->pHWDeviceCtx.get());
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
                            d->pInputHWFramesCtx.reset(pFramesCtx);
                        }
                    }

                    auto pFramesCtx = av_hwframe_ctx_alloc(d->pHWDeviceCtx.get());
                    auto framesContext = reinterpret_cast<AVHWFramesContext *>(pFramesCtx->data);
                    framesContext->width = d->currentFrame->width;
                    framesContext->height = d->currentFrame->height;
                    framesContext->format = AV_PIX_FMT_VAAPI;
                    framesContext->sw_format = AV_PIX_FMT_BGRA;
                    ret = av_hwframe_ctx_init(pFramesCtx);
                    if (0 != ret) {
                        qFatal("%d: Could not initialize output AVHWFramesContext: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                    d->pOutputHWFramesCtx.reset(pFramesCtx);

                    for (auto i = 0; i < d->pFilterGraph->nb_filters; ++i) {
                        d->pFilterGraph->filters[i]->hw_device_ctx = av_buffer_ref(d->pHWDeviceCtx.get());
                    }

                    char args[512];
                    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                             d->currentFrame->width, d->currentFrame->height, AV_PIX_FMT_VAAPI,
                             1, 1000000,
                             1, 1);

                    AVFilterContext *pBufferSrcCtx = nullptr;
                    ret = avfilter_graph_create_filter(&pBufferSrcCtx, avfilter_get_by_name("buffer"), "in",
                                                       args, nullptr, d->pFilterGraph.get());
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }
                    d->pBufferSrcCtx.reset(pBufferSrcCtx);
                    //            ret = avfilter_graph_create_filter(&d->pVAAPIDenoiseCtx, avfilter_get_by_name("denoise_vaapi"), "vaapi_denoise",
                    //                                               nullptr, nullptr, d->pFilterGraph);
                    //            if (ret != 0) {
                    //                qFatal("Could not link filters: %d", ret);
                    //            }
                    AVFilterContext *pVAAPIScaleCtx = nullptr;
                    ret = avfilter_graph_create_filter(&pVAAPIScaleCtx, avfilter_get_by_name("scale_vaapi"), "vaapi_scale",
                                                       "format=bgra", nullptr, d->pFilterGraph.get());
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }
                    d->pVAAPIScaleCtx.reset(pVAAPIScaleCtx);

                    AVFilterContext *pBufferSinkCtx = nullptr;
                    ret = avfilter_graph_create_filter(&pBufferSinkCtx, avfilter_get_by_name("buffersink"), "out",
                                                       nullptr, nullptr, d->pFilterGraph.get());
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }
                    d->pBufferSinkCtx.reset(pBufferSinkCtx);

                    ret = avfilter_link(d->pBufferSrcCtx.get(), 0, d->pInputs->filter_ctx, d->pInputs->pad_idx);
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }
                    ret = avfilter_link(d->pOutputs->filter_ctx, d->pOutputs->pad_idx, d->pVAAPIScaleCtx.get(), 0);
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }
                    //            ret = avfilter_link(d->pVAAPIScaleCtx, 0, d->pVAAPIDenoiseCtx, 0);
                    //            if (ret != 0) {
                    //                qFatal("Could not link filters: %d", ret);
                    //            }
                    ret = avfilter_link(d->pVAAPIScaleCtx.get(), 0, d->pBufferSinkCtx.get(), 0);
                    if (ret != 0) {
                        qFatal("Could not link filters: %d", ret);
                    }

                    d->pBufferSrcCtx->outputs[0]->hw_frames_ctx = av_buffer_ref(d->pInputHWFramesCtx.get());
                    //            d->pVAAPIDenoiseCtx->inputs[0]->hw_frames_ctx = av_buffer_ref(d->pInputHWFramesCtx);
                    //            d->pVAAPIDenoiseCtx->outputs[0]->hw_frames_ctx = av_buffer_ref(d->pInputHWFramesCtx);
                    d->pVAAPIScaleCtx->inputs[0]->hw_frames_ctx = av_buffer_ref(d->pInputHWFramesCtx.get());
                    d->pVAAPIScaleCtx->outputs[0]->hw_frames_ctx = av_buffer_ref(d->pOutputHWFramesCtx.get());
                    d->pBufferSinkCtx->inputs[0]->hw_frames_ctx = av_buffer_ref(d->pOutputHWFramesCtx.get());

                    ret = avfilter_graph_config(d->pFilterGraph.get(), nullptr);
                    if (ret != 0) {
                        qFatal("%d: Could not configure filter graph: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                    }
                }

                std::shared_ptr<AVFrame> output(av_frame_alloc(), [](AVFrame *p) {
                    av_frame_free(&p);
                });

                ret = av_buffersrc_add_frame(d->pBufferSrcCtx.get(), d->currentFrame.get());
                if (ret != 0) {
                    qFatal("%d: Could not enqueue frame into filter graph: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }
                ret = av_buffersink_get_frame(d->pBufferSinkCtx.get(), output.get());
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    continue;
                } else if (ret != 0) {
                    qFatal("%d: Could not getFBO frame from filter graph: %s", ret, av_make_error_string(strBuf, strBufSize, ret));
                }
                output->opaque = d->currentFrame->opaque;

                produce(communication::Message::builder().withAction(communication::Message::Action::DATA).withPayload("frame", QVariant::fromValue(output)).build(), d->outputPadId);
            }
        }
    }

    void VaapiYuvToRgbMapperPrivate::destroyAVFilterInOut(AVFilterInOut *filterInOut) {
        if (filterInOut) {
            avfilter_inout_free(&filterInOut);
        }
    }

    void VaapiYuvToRgbMapperPrivate::destroyAVFilterGraph(AVFilterGraph *filterGraph) {
        if (filterGraph) {
            avfilter_graph_free(&filterGraph);
        }
    }

    void VaapiYuvToRgbMapperPrivate::destroyAVFilterContext(AVFilterContext *filterContext) {
        if (filterContext) {
            avfilter_free(filterContext);
        }
    }

    void VaapiYuvToRgbMapperPrivate::destroyAVBufferRef(AVBufferRef *ref) {
        if (ref) {
            av_buffer_unref(&ref);
        }
    }

}// namespace AVQt
