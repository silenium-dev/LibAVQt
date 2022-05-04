#include "include/AVQt/common/AudioFormat.hpp"

namespace AVQt::common {
    AudioFormat::AudioFormat(int sampleRate, int channels, AVSampleFormat sampleFormat)
        : m_sampleRate(sampleRate),
          m_channels(channels),
          m_format(sampleFormat),
          m_channelLayout(AV_CH_LAYOUT_NATIVE),
          m_bytesPerSample(av_get_bytes_per_sample(m_format)) {
    }

    AudioFormat::AudioFormat(int sampleRate, int channels, AVSampleFormat sampleFormat, uint64_t channelLayout)
        : m_sampleRate(sampleRate),
          m_channels(channels),
          m_format(sampleFormat),
          m_channelLayout(channelLayout),
          m_bytesPerSample(av_get_bytes_per_sample(m_format)) {
    }

    AVSampleFormat AudioFormat::sampleFormat() const {
        return m_format;
    }

    __int128 AudioFormat::channelLayout() const {
        return m_channelLayout;
    }

    int AudioFormat::channels() const {
        return m_channels;
    }

    int AudioFormat::sampleRate() const {
        return m_sampleRate;
    }

    int AudioFormat::bytesPerSample() const {
        return m_bytesPerSample;
    }

    bool AudioFormat::operator==(const AudioFormat &other) const {
        return (m_sampleRate == other.m_sampleRate || m_sampleRate == -1 || other.m_sampleRate == -1) &&
               (m_channels == other.m_channels || m_channels == -1 || other.m_channels == -1) &&
               (m_format == other.m_format || m_format == -1 || other.m_format == -1) &&
               (m_channelLayout == other.m_channelLayout || m_channelLayout == -1 || other.m_channelLayout == -1);
    }

    bool AudioFormat::operator!=(const AudioFormat &other) const {
        return !(*this == other);
    }

    bool AudioFormat::isSupportedBy(QList<AudioFormat> supportedFormats) const {
        return supportedFormats.isEmpty() ||
               std::any_of(supportedFormats.begin(), supportedFormats.end(), [this](const AudioFormat &format) {
                   return *this == format;
               });
    }

    QString AudioFormat::toString() const {
        char strBuf[256];
        if (m_channelLayout >= 0) {
            av_get_channel_layout_string(strBuf, sizeof(strBuf), m_channels, m_channelLayout);
        }
        return QString("%1 Hz, %2 channels, %3, %4").arg(QString::number(m_sampleRate), QString::number(m_channels), av_get_sample_fmt_name(m_format), strBuf);
    }
}// namespace AVQt::common
