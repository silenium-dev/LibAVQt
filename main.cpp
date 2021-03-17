#include "AVQt/AVQt"

#include <QtWidgets>
#include <csignal>

QCoreApplication *app;

void signalHandler(int sigNum) {
    Q_UNUSED(sigNum)
    QApplication::quit();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        qFatal("No output file given");
    }

    signal(SIGINT, &signalHandler);
    signal(SIGTERM, &signalHandler);
    signal(SIGQUIT, &signalHandler);

    app = new QApplication(argc, argv);

    QFile file(QFileDialog::getOpenFileName(nullptr, "Select input video file",
                                            QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0],
                                            "Video files (*.mp4 *.mkv *.webm, *.m4v)"));
    if (!file.exists()) {
        exit(0);
    }
    file.open(QIODevice::ReadOnly);

    auto *framesaver = new AVQt::FrameFileSaver(60, "main");

    auto *decoderVaapi = new AVQt::DecoderVAAPI(&file, nullptr);

//    decoderVaapi->registerCallback(framesaver, AVQt::IFrameSource::CB_QIMAGE);

    auto renderer = new AVQt::OpenGLWidgetRenderer();

    renderer->setBaseSize(1280, 720);
    renderer->showNormal();

//    auto encoderVaapi = new AVQt::EncoderVAAPI(new QFile(app->arguments()[2]), AVQt::EncoderVAAPI::VIDEO_CODEC::H264,
//                                               AVQt::EncoderVAAPI::AUDIO_CODEC::AAC, 1920, 1080, av_make_q(25, 1), 10 * 1024 * 1024);

//    encoderVaapi->init();

//    decoderVaapi->registerCallback(encoderVaapi, AVQt::IFrameSource::CB_AVFRAME);
    decoderVaapi->registerCallback(renderer, AVQt::IFrameSource::CB_AVFRAME);

    decoderVaapi->init();
//    encoderVaapi->start();

    renderer->start();
    decoderVaapi->start();

    QObject::connect(app, &QCoreApplication::aboutToQuit, [&] {
        qDebug() << "Stopping...";
        renderer->stop();
        decoderVaapi->stop();
        decoderVaapi->wait(100);
//        encoderVaapi->stop();
        decoderVaapi->deinit();
        delete decoderVaapi;
        delete renderer;
//        delete encoderVaapi;
        delete framesaver;
    });


    return QCoreApplication::exec();
}
