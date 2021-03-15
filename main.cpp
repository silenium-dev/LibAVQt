#include "AVQt/input/DecoderVAAPI.h"
#include "AVQt/output/EncoderVAAPI.h"
#include "AVQt/output/FrameFileSaver.h"

#include <QtCore>
#include <csignal>

QCoreApplication *app;

void signalHandler(int sigNum) {
    Q_UNUSED(sigNum)
    QCoreApplication::quit();
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        qFatal("No input and output file given");
    }

    signal(SIGINT, &signalHandler);
    signal(SIGTERM, &signalHandler);
    signal(SIGQUIT, &signalHandler);

    app = new QCoreApplication(argc, argv);

    QFile file(app->arguments()[1]);
    file.open(QIODevice::ReadWrite);

    auto *framesaver = new AVQt::FrameFileSaver(60, "main");

    auto *decoderVaapi = new AVQt::DecoderVAAPI(&file, nullptr);

    decoderVaapi->registerCallback(framesaver, AVQt::IFrameSource::CB_QIMAGE);

    auto encoderVaapi = new AVQt::EncoderVAAPI(new QFile(app->arguments()[2]), AVQt::EncoderVAAPI::VIDEO_CODEC::H264,
                                               AVQt::EncoderVAAPI::AUDIO_CODEC::AAC, 1920, 1080, av_make_q(25, 1), 10 * 1024 * 1024);

    encoderVaapi->init();

    decoderVaapi->registerCallback(encoderVaapi, AVQt::IFrameSource::CB_AVFRAME);

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
