#ifndef LIBAVQT_AUDIOPADPARAMS_HPP
#define LIBAVQT_AUDIOPADPARAMS_HPP

#include "AVQt/common/AudioFormat.hpp"

#include <pgraph/api/PadUserData.hpp>

namespace AVQt::communication {
    class AudioPadParams : public pgraph::api::PadUserData {
    public:
        explicit AudioPadParams(const common::AudioFormat &format);
        AudioPadParams(const AudioPadParams &other) = default;
        AudioPadParams &operator=(const AudioPadParams &other) = default;
        AudioPadParams(AudioPadParams &&other) = default;
        AudioPadParams &operator=(AudioPadParams &&other) = default;
        ~AudioPadParams() override = default;

        QUuid getType() const override;
        QJsonObject toJSON() const override;

    protected:
        bool isEmpty() const override;

    public:
        static const QUuid Type;

        common::AudioFormat format;
    };
}// namespace AVQt::communication


#endif//LIBAVQT_AUDIOPADPARAMS_HPP
