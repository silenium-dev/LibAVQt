#include "AVQt"

#include <QApplication>
#include <csignal>
#include <QFileDialog>

QApplication *app = nullptr;

void signalHandler(int sigNum) {
    Q_UNUSED(sigNum)
    QApplication::quit();
}

int main(int argc, char *argv[]) {
    app = new QApplication(argc, argv);
    signal(SIGINT, &signalHandler);
    signal(SIGTERM, &signalHandler);
    signal(SIGQUIT, &signalHandler);

    auto inputFile = new QFile(QFileDialog::getOpenFileName(nullptr, "Select video file",
                                                            QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0],
                                                            "Video file (*.mkv *.mp4 *.webm *.m4v *.ts)"));

    if (inputFile->fileName().isEmpty()) {
        return 0;
    }

    inputFile->open(QIODevice::ReadOnly);

    AVQt::Demuxer demuxer(inputFile);
//    AVQt::AudioDecoder decoder;
//    AVQt::OpenALAudioOutput output;

//    demuxer.registerCallback(&decoder, AVQt::IPacketSource::CB_AUDIO);
//    decoder.registerCallback(&output);

    AVQt::DecoderVAAPI decoderVaapi;
    AVQt::OpenGLRenderer renderer;

    demuxer.registerCallback(&decoderVaapi, AVQt::IPacketSource::CB_VIDEO);
    decoderVaapi.registerCallback(&renderer);

    renderer.setMinimumSize(QSize(360, 240));

//    QObject::connect(&renderer, &AVQt::OpenGLRenderer::paused, [&](bool paused) {
//        output.pause(nullptr, paused);
//    });
//    QObject::connect(&renderer, &AVQt::OpenGLRenderer::started, [&]() {
//        output.start(nullptr);
//    });

    demuxer.init();
    demuxer.start();

    QObject::connect(app, &QApplication::aboutToQuit, [&] {
        demuxer.deinit();
    });

    return QApplication::exec();
}