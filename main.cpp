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

    inputFile->open(QIODevice::ReadOnly);

    AVQt::Demuxer demuxer(inputFile);
    AVQt::DecoderVAAPI decoderVaapi;
    AVQt::OpenGLRenderer renderer;

    demuxer.registerCallback(&decoderVaapi, AVQt::IPacketSource::CB_VIDEO);

    decoderVaapi.registerCallback(&renderer);

    renderer.setMinimumSize(QSize(360, 240));
    renderer.showNormal();

    demuxer.init();
    demuxer.start();

    QObject::connect(app, &QApplication::aboutToQuit, [&] {
        demuxer.deinit();
    });

    return QApplication::exec();
}