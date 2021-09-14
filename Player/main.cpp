#include "AVQt"

#include <QApplication>
#include <QFileDialog>
#include <csignal>
#include <iostream>
#include <qglobal.h>

constexpr auto LOGFILE_LOCATION = "libAVQt.log";

QApplication *app = nullptr;
std::chrono::time_point<std::chrono::system_clock> start; // NOLINT(cert-err58-cpp)

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

    auto now = QDateTime::currentDateTime();

    os << now.toString(Qt::ISODateWithMs) << ": ";
    os << qPrintable(qFormatLogMessage(type, context, message)) << "\n";

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

    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    //    signal(SIGQUIT, &signalHandler);

    start = std::chrono::system_clock::now();

    qInstallMessageHandler(&messageHandler);

    QString filepath;

    if (argc < 2) {
        filepath = QFileDialog::getOpenFileName(nullptr, "Select video file",
                                                QStandardPaths::standardLocations(QStandardPaths::HomeLocation).at(0),
                                                "Video file (*.mkv *.mp4 *.webm *.m4v *.ts)");
        if (filepath.isEmpty()) {
            return 0;
        }
    } else if (QFile::exists(argv[1])) {
        filepath = argv[1];
    }

    auto inputFile = new QFile(filepath);

    //    if (inputFile->fileName().isEmpty()) {
    //        return 0;
    //    }

    inputFile->open(QIODevice::ReadWrite);

    AVQt::Demuxer *demuxer = new AVQt::Demuxer(inputFile);
    AVQt::AudioDecoder decoder;
    AVQt::OpenALAudioOutput output;

    demuxer.registerCallback(&decoder, AVQt::IPacketSource::CB_AUDIO);
    decoder.registerCallback(&output);

    AVQt::IDecoder *videoDecoder = nullptr;
    AVQt::IEncoder *videoEncoder = nullptr;
#ifdef Q_OS_LINUX
    videoDecoder = new AVQt::DecoderVAAPI;
#elif defined(Q_OS_WINDOWS)
    videoDecoder = new AVQt::DecoderD3D11VA();
    videoEncoder = new AVQt::EncoderQSV(AVQt::IEncoder::CODEC::HEVC, 10 * 1000 * 1000);
#else
#error "Unsupported OS"
#endif
    AVQt::OpenGLRenderer renderer;

    demuxer->registerCallback(videoDecoder, AVQt::IPacketSource::CB_VIDEO);
//    QObject::connect(&renderer, &AVQt::OpenGLRenderer::frameProcessingStarted, &output, &AVQt::OpenALAudioOutput::enqueueAudioForFrame);
#ifdef ENABLE_QSV_ENCODE
    videoEncoder = new AVQt::EncoderQSV(AVQt::IEncoder::CODEC::HEVC, 10 * 1000 * 1000);
    videoDecoder->registerCallback(videoEncoder);
#endif
    //    QFile outputFile("output.mp4");
    //    outputFile.open(QIODevice::ReadWrite | QIODevice::Truncate);
    //    outputFile.seek(0);
    //    AVQt::Muxer muxer(&outputFile, AVQt::Muxer::FORMAT::MP4);

    //    videoEncoder->registerCallback(&muxer, AVQt::IPacketSource::CB_VIDEO);
    videoDecoder->registerCallback(&renderer);

    renderer.setMinimumSize(QSize(360, 240));

    //    QObject::connect(&renderer, &AVQt::OpenGLRenderer::paused, [&](bool paused) {
    //        output.pause(nullptr, paused);
    //    });
    //    QObject::connect(&renderer, &AVQt::OpenGLRenderer::started, [&]() {
    //        output.start(nullptr);
    //    });

    demuxer->init();

    QObject::connect(&renderer, &AVQt::OpenGLRenderer::frameProcessingStarted, &output, &AVQt::OpenALAudioOutput::enqueueAudioForFrame, Qt::QueuedConnection);
    QObject::connect(&renderer, &AVQt::OpenGLRenderer::paused, demuxer, &AVQt::Demuxer::pause);

    demuxer->start();

    QObject::connect(app, &QApplication::aboutToQuit, [demuxer, videoDecoder, videoEncoder] {
        demuxer->deinit();
        //        muxer.deinit(videoEncoder);
        delete videoEncoder;
        delete videoDecoder;
    });

    return QApplication::exec();
}