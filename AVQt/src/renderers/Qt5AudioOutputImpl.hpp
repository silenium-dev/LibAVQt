#ifndef LIBAVQT_QT6AUDIOOUTPUTIMPL_HPP
#define LIBAVQT_QT6AUDIOOUTPUTIMPL_HPP

#include "renderers/IAudioOutputImpl.hpp"

#include <QThread>

namespace AVQt {
    class Qt5AudioOutputImplPrivate;
    class Qt5AudioOutputImpl : public QThread, public api::IAudioOutputImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(Qt5AudioOutputImpl)
        Q_INTERFACES(AVQt::api::IAudioOutputImpl)
    public:
        static const api::AudioOutputImplInfo info();

        Q_INVOKABLE explicit Qt5AudioOutputImpl(QObject *parent = nullptr);
        ~Qt5AudioOutputImpl() override = default;

        bool open(const communication::AudioPadParams &params) override;
        void close() override;

        void write(const std::shared_ptr<AVFrame> &frame) override;

        void resetBuffer() override;
        void pause(bool state) override;

    protected:
        void run() override;

    private:
        std::unique_ptr<Qt5AudioOutputImplPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_QT6AUDIOOUTPUTIMPL_HPP
