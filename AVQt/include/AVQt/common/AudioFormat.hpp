#ifndef LIBAVQT_AUDIOFORMAT_HPP
#define LIBAVQT_AUDIOFORMAT_HPP

#include <QObject>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

namespace AVQt::common {
    class AudioFormat {
    public:
        AudioFormat(int sampleRate, int channels, AVSampleFormat sampleFormat);
        AudioFormat(int sampleRate, int channels, AVSampleFormat sampleFormat, uint64_t channelLayout);
        AudioFormat(const AudioFormat &other) = default;
        AudioFormat &operator=(const AudioFormat &other) = default;
        AudioFormat(AudioFormat &&other) = default;
        AudioFormat &operator=(AudioFormat &&other) = default;

        [[nodiscard]] int sampleRate() const;
        [[nodiscard]] int channels() const;
        [[nodiscard]] int bytesPerSample() const;
        [[nodiscard]] AVSampleFormat sampleFormat() const;
        [[nodiscard]] uint64_t channelLayout() const;

        bool operator==(const AudioFormat &other) const;
        bool operator!=(const AudioFormat &other) const;

    private:
        AVSampleFormat m_format;
        int m_channels;
#if NO_INT128
        int64_t m_channelLayout;
#else
        __int128 m_channelLayout;
#endif
        int m_sampleRate;
        int m_bytesPerSample;
    };
}// namespace AVQt::common


#endif//LIBAVQT_AUDIOFORMAT_HPP
