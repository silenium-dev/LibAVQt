#ifndef LIBAVQT_QT6AUDIOOUTPUTIMPL_HPP
#define LIBAVQT_QT6AUDIOOUTPUTIMPL_HPP

#include "renderers/IAudioOutputImpl.hpp"

#include <QThread>

namespace AVQt {
    class Qt6AudioOutputImplPrivate;
    class Qt6AudioOutputImpl : public QThread, public api::IAudioOutputImpl {
        Q_OBJECT
        Q_DECLARE_PRIVATE(Qt6AudioOutputImpl)
        Q_INTERFACES(AVQt::api::IAudioOutputImpl)
    public:
        static const api::AudioOutputImplInfo info();

        Q_INVOKABLE explicit Qt6AudioOutputImpl(QObject *parent = nullptr);
        ~Qt6AudioOutputImpl() override = default;

        bool open(const communication::AudioPadParams &params) override;
        void close() override;

        void write(const std::shared_ptr<AVFrame> &frame) override;

        void resetBuffer() override;
        void pause(bool state) override;

    protected:
        void run() override;

    private:
        std::unique_ptr<Qt6AudioOutputImplPrivate> d_ptr;
    };
}// namespace AVQt


#endif//LIBAVQT_QT6AUDIOOUTPUTIMPL_HPP
