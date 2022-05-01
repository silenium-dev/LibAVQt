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

    uint64_t AudioFormat::channelLayout() const {
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
}// namespace AVQt::common
