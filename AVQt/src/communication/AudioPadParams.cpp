#include "communication/AudioPadParams.hpp"

namespace AVQt::communication {
    const QUuid AudioPadParams::Type = QUuid::fromString(QLatin1String{"06990d2cbfa0bd26e27574ef26d75cd7"});

    AudioPadParams::AudioPadParams(const common::AudioFormat &format)
        : format(format) {
    }

    QUuid AudioPadParams::getType() const {
        return Type;
    }

    bool AudioPadParams::isEmpty() const {
        return false;
    }

    QJsonObject AudioPadParams::toJSON() const {
        QJsonObject obj, data;
        obj["type"] = Type.toString();

        QJsonObject formatObj;
        formatObj["sampleFormat"] = format.sampleFormat();
        formatObj["sampleRate"] = format.sampleRate();
        formatObj["channels"] = format.channels();
        formatObj["channelLayout"] = QJsonValue::fromVariant(QVariant::fromValue(format.channelLayout()));// Needed, directly inserting quint64 results in ambiguous constructor call
        formatObj["bytesPerSample"] = format.bytesPerSample();

        data["format"] = formatObj;
        obj["data"] = data;

        return obj;
    }
}// namespace AVQt::communication
