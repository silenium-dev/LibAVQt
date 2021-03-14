#include "AV/input/DecoderVAAPI.h"
#include "AV/output/EncoderVAAPI.h"
#include "AV/output/FrameSaver.h"

#include <QtCore>
#include <csignal>

QCoreApplication *app;

void signalHandler(int sigNum) {
    Q_UNUSED(sigNum)
    QCoreApplication::quit();
}

int main(int argc, char *argv[]) {
    signal(SIGINT, &signalHandler);
    signal(SIGTERM, &signalHandler);
    signal(SIGQUIT, &signalHandler);

    app = new QCoreApplication(argc, argv);

    QFile file("/home/silas/1080p60.mkv");
    file.open(QIODevice::ReadWrite);

    auto *framesaver = new AV::FrameSaver(nullptr);

    auto *decoderVaapi = new AV::DecoderVAAPI(&file, nullptr);

//    decoderVaapi->registerCallback(framesaver, AV::IFrameSource::CB_QIMAGE);

    auto encoderVaapi = new AV::EncoderVAAPI(new QFile("output.ts"), AV::EncoderVAAPI::VIDEO_CODEC::H264,
                                             AV::EncoderVAAPI::AUDIO_CODEC::AAC, 1920, 1080, av_make_q(25, 1), 10 * 1024 * 1024);

    encoderVaapi->init();

    decoderVaapi->registerCallback(encoderVaapi, AV::IFrameSource::CB_AVFRAME);

    decoderVaapi->init();
    encoderVaapi->start();
    decoderVaapi->start();

    QObject::connect(app, &QCoreApplication::aboutToQuit, [&] {
        qDebug() << "Stopping...";
        decoderVaapi->stop();
        encoderVaapi->stop();
        decoderVaapi->deinit();
        delete decoderVaapi;
        delete encoderVaapi;
        delete framesaver;
    });


    return QCoreApplication::exec();
}
