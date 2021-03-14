//
// Created by silas on 3/14/21.
//

#include "private/OpenGLWidgetRenderer_p.h"
#include "OpenGLWidgetRenderer.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}


namespace AV {
    OpenGLWidgetRenderer::OpenGLWidgetRenderer(QWidget *parent) : QOpenGLWidget(parent), d_ptr(new OpenGLWidgetRendererPrivate(this)) {

    }

    OpenGLWidgetRenderer::OpenGLWidgetRenderer(OpenGLWidgetRendererPrivate &p) : d_ptr(&p) {

    }

    int OpenGLWidgetRenderer::init() {
        Q_D(AV::OpenGLWidgetRenderer);

        return 0;
    }

    int OpenGLWidgetRenderer::deinit() {
        return 0;
    }

    int OpenGLWidgetRenderer::start() {
        return 0;
    }

    int OpenGLWidgetRenderer::stop() {
        return 0;
    }

    int OpenGLWidgetRenderer::pause(bool paused) {
        return 0;
    }

    void OpenGLWidgetRenderer::onFrame(QImage frame, AVRational timebase, AVRational framerate) {

    }

    void OpenGLWidgetRenderer::onFrame(AVFrame *frame, AVRational timebase, AVRational framerate) {

    }
}
