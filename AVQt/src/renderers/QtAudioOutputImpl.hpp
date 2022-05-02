#ifndef LIBAVQT_QTAUDIOOUTPUTIMPL_HPP
#define LIBAVQT_QTAUDIOOUTPUTIMPL_HPP

#include "renderers/IAudioOutputImpl.hpp"

#include <QThread>

namespace AVQt {
    class QtAudioOutputImplPrivate;
    class QtAudioOutputImpl : public QThread, public api::IAudioOutputImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(QtAudioOutputImpl)
        Q_INTERFACES(AVQt::api::IAudioOutputImpl)
    public:
        static const api::AudioOutputImplInfo info();

        Q_INVOKABLE explicit QtAudioOutputImpl(QObject *parent = nullptr);
        ~QtAudioOutputImpl() override = default;

        bool open(const communication::AudioPadParams &params) override;
        void close() override;

        void write(const std::shared_ptr<AVFrame> &frame) override;

        void resetBuffer() override;

    protected:
        void run() override;

    private:
        std::unique_ptr<QtAudioOutputImplPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_QTAUDIOOUTPUTIMPL_HPP
