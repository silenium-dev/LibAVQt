#ifndef LIBAVQT_QTAUDIOOUTPUTIMPL_P_HPP
#define LIBAVQT_QTAUDIOOUTPUTIMPL_P_HPP

#include "common/AudioFormat.hpp"

#include <QObject>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>

#include <condition_variable>
#include <mutex>

namespace AVQt {
    class QtAudioOutputImpl;
    class QtAudioOutputImplPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(QtAudioOutputImpl)
    private slots:
        void trySendData();

    private:
        explicit QtAudioOutputImplPrivate(QtAudioOutputImpl *q);
        QtAudioOutputImpl *q_ptr;

        std::optional<common::AudioFormat> audioFormat{};
        std::unique_ptr<QAudioSink> audioSink{};
        QIODevice *audioOutputDevice{};

        static constexpr int bufferSize = 65536;
        std::mutex inputMutex{};
        std::condition_variable inputCond{};
        QByteArray inputBuffer{};

        const QMap<AVSampleFormat, QAudioFormat::SampleFormat> sampleFormatMap{
                {AV_SAMPLE_FMT_U8, QAudioFormat::SampleFormat::UInt8},
                {AV_SAMPLE_FMT_U8P, QAudioFormat::SampleFormat::UInt8},
                {AV_SAMPLE_FMT_S16, QAudioFormat::SampleFormat::Int16},
                {AV_SAMPLE_FMT_S16P, QAudioFormat::SampleFormat::Int16},
                {AV_SAMPLE_FMT_S32, QAudioFormat::SampleFormat::Int32},
                {AV_SAMPLE_FMT_S32P, QAudioFormat::SampleFormat::Int32},
                {AV_SAMPLE_FMT_FLT, QAudioFormat::SampleFormat::Float},
                {AV_SAMPLE_FMT_FLTP, QAudioFormat::SampleFormat::Float},
                {AV_SAMPLE_FMT_DBL, QAudioFormat::SampleFormat::Float},
                {AV_SAMPLE_FMT_DBLP, QAudioFormat::SampleFormat::Float},
        };

        const QList<AVSampleFormat> planarFormats{
                AV_SAMPLE_FMT_U8P,
                AV_SAMPLE_FMT_S16P,
                AV_SAMPLE_FMT_S32P,
                AV_SAMPLE_FMT_FLTP,
                AV_SAMPLE_FMT_DBLP,
        };

        const QMap<uint64_t, QAudioFormat::ChannelConfig> channelLayoutMap{
                {AV_CH_LAYOUT_MONO, QAudioFormat::ChannelConfig::ChannelConfigMono},
                {AV_CH_LAYOUT_STEREO, QAudioFormat::ChannelConfig::ChannelConfigStereo},
                {AV_CH_LAYOUT_2POINT1, QAudioFormat::ChannelConfig::ChannelConfig2Dot1},
                {AV_CH_LAYOUT_2_1, QAudioFormat::ChannelConfig::ChannelConfig2Dot1},
                {AV_CH_LAYOUT_5POINT0, QAudioFormat::ChannelConfig::ChannelConfigSurround5Dot0},
                {AV_CH_LAYOUT_5POINT1, QAudioFormat::ChannelConfig::ChannelConfigSurround5Dot1},
                {AV_CH_LAYOUT_7POINT0, QAudioFormat::ChannelConfig::ChannelConfigSurround7Dot0},
                {AV_CH_LAYOUT_7POINT1, QAudioFormat::ChannelConfig::ChannelConfigSurround7Dot1},
        };
    };
}// namespace AVQt


#endif//LIBAVQT_QTAUDIOOUTPUTIMPL_P_HPP
