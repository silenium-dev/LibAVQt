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
        explicit AudioFormat() = default;
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
#if NO_INT128
        [[nodiscard]] int64_t channelLayout() const;
#else
        [[nodiscard]] __int128_t channelLayout() const;
#endif

        [[nodiscard]] bool isSupportedBy(QList<AudioFormat> supportedFormats) const;

        [[nodiscard]] QString toString() const;

        bool operator==(const AudioFormat &other) const;
        bool operator!=(const AudioFormat &other) const;

    private:
        AVSampleFormat m_format{AV_SAMPLE_FMT_NONE};
        int m_channels{-1};
#if NO_INT128
        int64_t m_channelLayout{-1};
#else
        __int128 m_channelLayout{-1};
#endif
        int m_sampleRate{-1};
        int m_bytesPerSample{-1};
    };
}// namespace AVQt::common

#if not NO_INT128
Q_DECLARE_METATYPE(__int128)
#endif

#endif//LIBAVQT_AUDIOFORMAT_HPP
