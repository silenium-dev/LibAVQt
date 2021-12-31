// Copyright (c) 2021.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "AVQt"
#include "FrameSaverAccelerated.hpp"
#include "OpenGlWidgetRenderer.hpp"
#include "include/AVQt/communication/Message.hpp"
#include "include/AVQt/communication/PacketPadParams.hpp"
#include <pgraph/api/Link.hpp>
#include <pgraph_network/data/ApiInfo.hpp>
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
    static bool logFileIsOpen = logFile.open(QIODevice::Append);
    if (logFile.size() > 1024 * 1024) {
        logFile.remove();
        logFile.open(QIODevice::WriteOnly);
    }

    //#ifndef QT_DEBUG
    if (type > QtMsgType::QtDebugMsg)
    //#endif
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

    //#ifdef QT_DEBUG
    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    //#else
    //    av_log_set_level(AV_LOG_WARNING);
    //#endif
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

    AVQt::EncodeParameters encodeParams{};
    encodeParams.bitrate = 1000000;
    encodeParams.codec = AVQt::Codec::HEVC;

    auto demuxer = std::make_shared<AVQt::Demuxer>(inputFile, registry);
    auto decoder1 = std::make_shared<AVQt::Decoder>("VAAPI", registry);
    auto decoder2 = std::make_shared<AVQt::Decoder>("VAAPI", registry);
    auto encoder = std::make_shared<AVQt::Encoder>("VAAPI", encodeParams, registry);
    auto renderer1 = std::make_shared<OpenGLWidgetRenderer>(registry);
    auto renderer2 = std::make_shared<OpenGLWidgetRenderer>(registry);
    auto yuvrgbconverter = std::make_shared<AVQt::VaapiYuvToRgbMapper>(registry);
    //    auto frameSaver = std::make_shared<FrameSaverAccelerated>(registry);
    //    auto cc = std::make_shared<CommandConsumer>(registry);
    //    auto cc2 = std::make_shared<CommandConsumer>(registry);

    demuxer->init();
    decoder1->init();
    decoder2->init();
    encoder->init();
    renderer1->init();
    renderer2->init();
    yuvrgbconverter->init();
    //    frameSaver->init();
    //    cc->init();
    //    cc2->init();

    //    pgraph::network::data::APIInfo apiInfo(registry);

    //    std::cout << QJsonDocument::fromJson(QByteArray::fromStdString(apiInfo.toString())).toJson(QJsonDocument::Indented).toStdString() << std::endl;

    auto demuxerOutPad = demuxer->getOutputPads().begin()->second;
    auto decoder1InPad = decoder1->getInputPads().begin()->second;
    auto decoder1OutPad = decoder1->getOutputPads().begin()->second;
    auto decoder2InPad = decoder2->getInputPads().begin()->second;
    auto decoder2OutPad = decoder2->getOutputPads().begin()->second;
    auto encoderInPad = encoder->getInputPads().begin()->second;
    auto encoderOutPad = encoder->getOutputPads().begin()->second;
    auto renderer1InPad = renderer1->getInputPads().begin()->second;
    auto renderer2InPad = renderer2->getInputPads().begin()->second;
    //    auto ccInPad = cc->getInputPads().begin()->second;
    //    auto cc2InPad = cc2->getInputPads().begin()->second;
    auto yuvrgbconverterInPad = yuvrgbconverter->getInputPads().begin()->second;
    auto yuvrgbconverterOutPad = yuvrgbconverter->getOutputPads().begin()->second;
    //    auto frameSaverInPad = frameSaver->getInputPads().begin()->second;
    //    ccInPad->link(demuxerOutPad);
    //    cc2InPad->link(decoderOutPad);
    decoder1InPad->link(demuxerOutPad);
    encoderInPad->link(decoder1OutPad);
    //    decoder2InPad->link(encoderOutPad);
    //    decoder1OutPad->link(yuvrgbconverterInPad);
    renderer1InPad->link(decoder1OutPad);
    //    renderer2InPad->link(decoder1OutPad);
    //    yuvrgbconverterOutPad->link(frameSaverInPad);

    demuxer->open();

    renderer1->resize(1280, 720);
    //    renderer2->resize(1280, 720);

    demuxer->start();

    //    QTimer::singleShot(4000, [demuxer]{
    //        demuxer->pause(true);
    //        QTimer::singleShot(4000, [demuxer]{
    //            demuxer->pause(false);
    QTimer::singleShot(30000, [renderer1, renderer2] {
        renderer1->close();
        renderer2->close();
    });
    //        });
    //    });

    QObject::connect(app, &QApplication::aboutToQuit, [demuxer] {
        demuxer->close();
    });
    //    demuxer->pause(true);
    //    demuxer->close();

    return QApplication::exec();
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
    ////    QObject::connect(&renderer, &AVQt::OpenGLRendererOld::frameProcessingStarted, &output, &AVQt::OpenALAudioOutput::enqueueAudioForFrame);
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
    //    //    QObject::connect(&renderer, &AVQt::OpenGLRendererOld::paused, [&](bool paused) {
    //    //        output.pause(nullptr, paused);
    //    //    });
    //    //    QObject::connect(&renderer, &AVQt::OpenGLRendererOld::started, [&]() {
    //    //        output.start(nullptr);
    //    //    });
    //
    //    demuxer->open();
    //
    //    //    output.syncToOutput(renderer->getFrameSink());
    //    //    QObject::connect(renderer->getFrameSink(), &AVQt::OpenGLRendererOld::paused, demuxer, &AVQt::Demuxer::pause);
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
