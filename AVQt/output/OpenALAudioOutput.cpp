//
// Created by silas on 4/28/21.
//

#include "private/OpenALAudioOutput_p.h"
#include "OpenALAudioOutput.h"

#include "filter/private/OpenALErrorHandler.h"
#include "OpenGLRenderer.h"

namespace AVQt {
    OpenALAudioOutput::OpenALAudioOutput(QObject *parent) : QObject(parent), d_ptr(new OpenALAudioOutputPrivate(this)) {

        [[maybe_unused]] OpenALAudioOutput::OpenALAudioOutput(OpenALAudioOutputPrivate & p) : d_ptr(&p)
        {
        }

        OpenALAudioOutput::OpenALAudioOutput(OpenALAudioOutput && other)
        noexcept: d_ptr(other.d_ptr)
        {
            other.d_ptr = nullptr;
            d_ptr->q_ptr = this;
        }

        bool OpenALAudioOutput::isPaused() {
            return false;
        }

        int OpenALAudioOutput::init(IAudioSource *source, int64_t duration, int sampleRate) {
            return 0;
        }

    int OpenALAudioOutput::deinit(IAudioSource *source) {
        return 0;
    }

    int OpenALAudioOutput::start(IAudioSource *source) {
        return 0;
    }

    int OpenALAudioOutput::stop(IAudioSource *source) {
        return 0;
    }

    void OpenALAudioOutput::pause(IAudioSource *source, bool pause) {

    }

    void OpenALAudioOutput::onAudioFrame(IAudioSource *source, AVFrame *frame, uint32_t duration) {

    }
}