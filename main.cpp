#include <QtCore>
#include "AV/DecoderVAAPI.h"
#include "AV/EncoderVAAPI.h"
#include "AV/FrameSaver.h"

int main(int argc, char *argv[]) {
    qInfo() << "Hello, World!";

    auto *app = new QCoreApplication(argc, argv);

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

    QTimer::singleShot(20000, [&] {
        qDebug() << "Stopping...";
        decoderVaapi->stop();
        encoderVaapi->stop();
        decoderVaapi->deinit();
        delete decoderVaapi;
        delete encoderVaapi;
        delete framesaver;
        app->quit();
    });


    return app->exec();
}
