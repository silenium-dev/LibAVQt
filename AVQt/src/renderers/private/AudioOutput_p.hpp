#ifndef LIBAVQT_AUDIOOUTPUT_P_HPP
#define LIBAVQT_AUDIOOUTPUT_P_HPP

#include "communication/AudioPadParams.hpp"
#include "renderers/IAudioOutputImpl.hpp"

#include <pgraph/api/Pad.hpp>

#include <QObject>

#include <optional>

namespace AVQt {
    class AudioOutput;
    class AudioOutputPrivate {
        Q_DECLARE_PUBLIC(AudioOutput)
    private:
        explicit AudioOutputPrivate(AudioOutput *q);
        AudioOutput *q_ptr;

        int64_t inputPadId{pgraph::api::INVALID_PAD_ID};

        std::optional<communication::AudioPadParams> audioParams;
        std::shared_ptr<api::IAudioOutputImpl> impl{};

        std::atomic_bool initialized{false}, open{false}, running{false}, paused{false};
    };
}// namespace AVQt

#endif//LIBAVQT_AUDIOOUTPUT_P_HPP
