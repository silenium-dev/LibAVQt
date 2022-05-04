#ifndef LIBAVQT_QT6AUDIOOUTPUTIMPL_P_HPP
#define LIBAVQT_QT6AUDIOOUTPUTIMPL_P_HPP

#include "common/AudioFormat.hpp"

#include <QObject>
#include <QTimer>

#include <QAudioFormat>
#include <QAudioOutput>

#include <condition_variable>
#include <mutex>
#include <optional>

namespace AVQt {
    class Qt5AudioOutputImpl;
    class Qt5AudioOutputImplPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(Qt5AudioOutputImpl)
    private slots:
        void trySendData();

    private:
        explicit Qt5AudioOutputImplPrivate(Qt5AudioOutputImpl *q);
        Qt5AudioOutputImpl *q_ptr;

        std::optional<common::AudioFormat> audioFormat{};
        std::unique_ptr<QAudioOutput> audioOutput{};
        QIODevice *audioOutputDevice{};
        std::unique_ptr<QTimer> audioOutputTimer{};

        static constexpr int bufferSize = 65536;
        std::mutex inputMutex{};
        std::condition_variable inputCond{};
        QByteArray inputBuffer{};

        const QMap<AVSampleFormat, QAudioFormat::SampleType> sampleFormatMap{
                {AV_SAMPLE_FMT_U8, QAudioFormat::SampleType::UnSignedInt},
                {AV_SAMPLE_FMT_U8P, QAudioFormat::SampleType::UnSignedInt},
                {AV_SAMPLE_FMT_S16, QAudioFormat::SampleType::SignedInt},
                {AV_SAMPLE_FMT_S16P, QAudioFormat::SampleType::SignedInt},
                {AV_SAMPLE_FMT_S32, QAudioFormat::SampleType::SignedInt},
                {AV_SAMPLE_FMT_S32P, QAudioFormat::SampleType::SignedInt},
                {AV_SAMPLE_FMT_FLT, QAudioFormat::SampleType::Float},
                {AV_SAMPLE_FMT_FLTP, QAudioFormat::SampleType::Float},
                {AV_SAMPLE_FMT_DBL, QAudioFormat::SampleType::Float},
                {AV_SAMPLE_FMT_DBLP, QAudioFormat::SampleType::Float},
        };

        const QList<AVSampleFormat> planarFormats{
                AV_SAMPLE_FMT_U8P,
                AV_SAMPLE_FMT_S16P,
                AV_SAMPLE_FMT_S32P,
                AV_SAMPLE_FMT_FLTP,
                AV_SAMPLE_FMT_DBLP,
        };
    };
}// namespace AVQt


#endif//LIBAVQT_QT6AUDIOOUTPUTIMPL_P_HPP
