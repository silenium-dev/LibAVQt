#include "AVQt"

#include <QApplication>
#include <QFileDialog>
#include <csignal>
#include <iostream>

constexpr auto LOGFILE_LOCATION = "libAVQt.log";

QApplication *app = nullptr;
std::chrono::time_point<std::chrono::system_clock> start;

void signalHandler(int sigNum) {
    Q_UNUSED(sigNum)
    QApplication::quit();
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    static QMutex mutex;
    QMutexLocker lock(&mutex);

    static QFile logFile(LOGFILE_LOCATION);
    static bool logFileIsOpen = logFile.open(QIODevice::Append | QIODevice::Text);

    QString output;
    QTextStream os(&output);

    os << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ": ";
    os << qPrintable(qFormatLogMessage(type, context, message)) << Qt::endl;

    std::cerr << output.toStdString();

    if (logFileIsOpen) {
        logFile.write(output.toLocal8Bit());
        logFile.flush();
    }
}

int main(int argc, char *argv[]) {
    app = new QApplication(argc, argv);
    signal(SIGINT, &signalHandler);
    signal(SIGTERM, &signalHandler);
    signal(SIGQUIT, &signalHandler);

    start = std::chrono::system_clock::now();

    qInstallMessageHandler(&messageHandler);

    auto inputFile = new QFile(QFileDialog::getOpenFileName(nullptr, "Select video file",
                                                            QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0],
                                                            "Video file (*.mkv *.mp4 *.webm *.m4v *.ts)"));

    if (inputFile->fileName().isEmpty()) {
        return 0;
    }

    inputFile->open(QIODevice::ReadOnly);

    AVQt::Demuxer demuxer(inputFile);
    AVQt::AudioDecoder decoder;
    AVQt::OpenALAudioOutput output;

    demuxer.registerCallback(&decoder, AVQt::IPacketSource::CB_AUDIO);
    decoder.registerCallback(&output);

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

    output.syncToOutput(&renderer);

    demuxer.start();

    QObject::connect(app, &QApplication::aboutToQuit, [&] {
        demuxer.deinit();
    });

    return QApplication::exec();
}