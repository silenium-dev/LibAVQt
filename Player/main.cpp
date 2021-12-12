#include "AVQt"
#include "include/AVQt/communication/Message.hpp"
#include "include/AVQt/communication/PacketPadParams.hpp"
#include "include/AVQt/input/CommandConsumer.hpp"
#include <pgraph/api/Link.hpp>
#include <pgraph_network/data/ApiInfo.hpp>
#include <pgraph_network/impl/RegisteringPadFactory.hpp>
#include <pgraph_network/impl/SimplePadRegistry.hpp>

#include <QApplication>
#include <QFileDialog>
#include <csignal>
#include <iostream>
#include <qglobal.h>

constexpr auto LOGFILE_LOCATION = "libAVQt.log";

QApplication *app = nullptr;
std::chrono::time_point<std::chrono::system_clock> start;// NOLINT(cert-err58-cpp)

void signalHandler(int sigNum) {
    Q_UNUSED(sigNum)
    QApplication::quit();
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    static QMutex mutex;
    QMutexLocker lock(&mutex);

    static QFile logFile(LOGFILE_LOCATION);
    static bool logFileIsOpen = logFile.open(QIODevice::Append | QIODevice::Text);

#ifndef QT_DEBUG
    if (type > QtMsgType::QtDebugMsg)
#endif
    {
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
}

int main(int argc, char *argv[]) {
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, false);
#ifdef Q_OS_WINDOWS
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
    SetErrorMode(GetErrorMode() | SEM_NOGPFAULTERRORBOX);
#endif
    app = new QApplication(argc, argv);
    signal(SIGINT, &signalHandler);
    signal(SIGTERM, &signalHandler);

#ifdef QT_DEBUG
    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
#endif
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

    auto registry = std::make_shared<pgraph::network::impl::SimplePadRegistry>();

    auto demuxer = std::make_shared<AVQt::Demuxer>(inputFile, registry);
    auto decoder = std::make_shared<AVQt::Decoder>("VAAPI", registry);
    auto cc = std::make_shared<CommandConsumer>(registry);

    demuxer->open();
    decoder->init();
    cc->open();

    //    cc->getInputPads().begin()->second->link(fileInput->getOutputPad(fileInput->getCommandOutputPadId()));

    pgraph::network::data::APIInfo apiInfo(registry);

    std::cout << QJsonDocument::fromJson(QByteArray::fromStdString(apiInfo.toString())).toJson(QJsonDocument::Indented).toStdString() << std::endl;

    auto demuxerOutPad = demuxer->getOutputPads().begin()->second;
    auto ccInPad = cc->getInputPads().begin()->second;
    auto decoderInPad = decoder->getInputPads().begin()->second;
//    ccInPad->link(demuxerOutPad);
    decoderInPad->link(demuxerOutPad);

    demuxer->init();
    demuxer->start();

    QThread::msleep(10);
    demuxer->pause(true);
    QThread::sleep(1);
    demuxer->pause(false);
    QThread::msleep(4);
    demuxer->close();
    //    demuxer->pause(true);
    //    demuxer->close();

    exit(0);
    //    AVQt::AudioDecoder decoder;
    //    AVQt::OpenALAudioOutput output;

    //    demuxer->registerCallback(&decoder, AVQt::IPacketSource::CB_AUDIO);
    //    decoder.registerCallback(&output);

    //    AVQt::IDecoderOld *videoDecoder = nullptr;
    //    AVQt::IEncoder *videoEncoder = nullptr;
    //#ifdef Q_OS_LINUX
    //    videoDecoder = new AVQt::Decoder;
    //#elif defined(Q_OS_WINDOWS)
    //    videoDecoder = new AVQt::DecoderD3D11VA();
    ////    videoEncoder = new AVQt::EncoderQSV(AVQt::IEncoder::CODEC::HEVC, 10 * 1000 * 1000);
    //#else
    //#error "Unsupported OS"
    //#endif
    //    auto renderer = new AVQt::OpenGLWidgetRenderer;
    //
    //    demuxer->registerCallback(videoDecoder, AVQt::IPacketSource::CB_VIDEO);
    ////    QObject::connect(&renderer, &AVQt::OpenGLRenderer::frameProcessingStarted, &output, &AVQt::OpenALAudioOutput::enqueueAudioForFrame);
    //#ifdef ENABLE_QSV_ENCODE
    //    videoEncoder = new AVQt::EncoderVAAPI(AVQt::IEncoder::CODEC::HEVC, 10 * 1000 * 1000);
    //    videoDecoder->registerCallback(videoEncoder);
    //#endif
    //    //    QFile outputFile("output.mp4");
    //    //    outputFile.open(QIODevice::ReadWrite | QIODevice::Truncate);
    //    //    outputFile.seek(0);
    //    //    AVQt::Muxer muxer(&outputFile, AVQt::Muxer::FORMAT::MP4);
    //
    //    //    videoEncoder->registerCallback(&muxer, AVQt::IPacketSource::CB_VIDEO);
    //    videoDecoder->registerCallback(renderer->getFrameSink());
    //
    //    renderer->setMinimumSize(QSize(360, 240));
    //    renderer->setAttribute(Qt::WA_QuitOnClose);
    //    renderer->setQuitOnClose(true);
    //    renderer->showNormal();
    //
    //    //    QObject::connect(&renderer, &AVQt::OpenGLRenderer::paused, [&](bool paused) {
    //    //        output.pause(nullptr, paused);
    //    //    });
    //    //    QObject::connect(&renderer, &AVQt::OpenGLRenderer::started, [&]() {
    //    //        output.start(nullptr);
    //    //    });
    //
    //    demuxer->open();
    //
    //    //    output.syncToOutput(renderer->getFrameSink());
    //    //    QObject::connect(renderer->getFrameSink(), &AVQt::OpenGLRenderer::paused, demuxer, &AVQt::Demuxer::pause);
    //
    //    demuxer->start();
    //
    //    QObject::connect(app, &QApplication::aboutToQuit, [demuxer, videoDecoder, videoEncoder, renderer] {
    //        demuxer->stop();
    //        demuxer->close();
    //        renderer->getFrameSink()->stop(videoDecoder);
    //        //        muxer.close(videoEncoder);
    //        delete renderer;
    //        delete videoEncoder;
    //        delete videoDecoder;
    //    });

    //    return QApplication::exec();
}
