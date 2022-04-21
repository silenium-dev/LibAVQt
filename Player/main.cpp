// Copyright (c) 2021-2022.
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

#include "FrameSaverAccelerated.hpp"
#include "OpenGlWidgetRenderer.hpp"

#include <AVQt/AVQt>
#include <pgraph/api/Link.hpp>
#include <pgraph/api/Pad.hpp>
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

        //        if (logFileIsOpen) {
        //            logFile.write(output.toLocal8Bit());
        //            logFile.flush();
        //        }
    }
}

int main(int argc, char *argv[]) {
    //    AVQt::registerAll();
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, false);
    QApplication::setAttribute(Qt::AA_UseOpenGLES, true);
#ifdef Q_OS_WINDOWS
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
    SetErrorMode(GetErrorMode() | SEM_NOGPFAULTERRORBOX);
#endif
    app = new QApplication(argc, argv);
    signal(SIGINT, &signalHandler);
    signal(SIGTERM, &signalHandler);

    //    QSurfaceFormat defaultFormat = QSurfaceFormat::defaultFormat();
    //    defaultFormat.setOption(QSurfaceFormat::DebugContext);
    //    QSurfaceFormat::setDefaultFormat(defaultFormat);

    //#ifdef QT_DEBUG
    //    av_log_set_level(AV_LOG_DEBUG);
    //    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    //#else
    av_log_set_level(AV_LOG_WARNING);
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

    //    if (inputFile->fileName().isEmpty()) {
    //        return 0;
    //    }
    auto registry = std::make_shared<pgraph::network::impl::SimplePadRegistry>();

    AVQt::EncodeParameters encodeParams{};
    encodeParams.bitrate = 10000000;
    //    encodeParams.codec = AVQt::Codec::H264;
    AVQt::VideoEncoder::Config encoderConfig;
    encoderConfig.codec = AVQt::Codec::VP8;
    encoderConfig.encodeParameters = encodeParams;
    encoderConfig.encoderPriority << "VAAPI";

    AVQt::Demuxer::Config demuxerConfig{};
    demuxerConfig.inputDevice = std::make_unique<QFile>(filepath);
    demuxerConfig.inputDevice->open(QIODevice::ReadWrite);
    demuxerConfig.loop = true;

    auto *buffer = new QFile("output.mkv");
    buffer->open(QIODevice::WriteOnly);

    auto demuxer = std::make_shared<AVQt::Demuxer>(std::move(demuxerConfig), registry);

    AVQt::Muxer::Config muxerConfig{};
    muxerConfig.outputDevice = std::unique_ptr<QIODevice>(buffer);
    muxerConfig.containerFormat = AVQt::common::ContainerFormat::MKV;
    auto muxer = std::make_shared<AVQt::Muxer>(std::move(muxerConfig), registry);

    AVQt::VideoDecoder::Config videoDecoderConfig{};
    videoDecoderConfig.decoderPriority << "VAAPI";
    auto decoder1 = std::make_shared<AVQt::VideoDecoder>(videoDecoderConfig, registry);
    auto decoder2 = std::make_shared<AVQt::VideoDecoder>(videoDecoderConfig, registry);
    //    auto decoder3 = std::make_shared<AVQt::VideoDecoder>("VAAPI", registry);
    auto encoder1 = std::make_shared<AVQt::VideoEncoder>(encoderConfig, registry);
    auto encoder2 = std::make_shared<AVQt::VideoEncoder>(encoderConfig, registry);
    auto renderer1 = std::make_shared<OpenGLWidgetRenderer>(registry);
    //    auto renderer2 = std::make_shared<OpenGLWidgetRenderer>(registry);
    auto yuvrgbconverter = std::make_shared<AVQt::VaapiYuvToRgbMapper>(registry);
    //    auto frameSaver = std::make_shared<FrameSaverAccelerated>(registry);
    auto cc = std::make_shared<CommandConsumer>(registry);
    //    auto cc2 = std::make_shared<CommandConsumer>(registry);

    //    auto decImpl = AVQt::VideoDecoderFactory::getInstance().create({AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE}, AV_CODEC_ID_H264, {"V4L2"});
    //    qDebug() << "decImpl" << decImpl.get();
    {
        demuxer->init();
        muxer->init();
        //    transcoder->init();
        decoder1->init();
        decoder2->init();
        //    decoder3->init();
        encoder1->init();
        encoder2->init();
        renderer1->init();
        //    renderer2->init();
        yuvrgbconverter->init();
        //    frameSaver->init();
        cc->init();
        //    cc2->init();

        //    pgraph::network::data::APIInfo apiInfo(registry);

        //    std::cout << QJsonDocument::fromJson(QByteArray::fromStdString(apiInfo.toString())).toJson(QJsonDocument::Indented).toStdString() << std::endl;

        std::shared_ptr<pgraph::api::Pad> demuxerOutPad{};
        auto demuxerPads = demuxer->getOutputPads();
        for (const auto &pad : demuxerPads) {
            if (pad.second->getUserData()->getType() == AVQt::communication::PacketPadParams::Type) {
                const auto padParams = std::dynamic_pointer_cast<const AVQt::communication::PacketPadParams>(pad.second->getUserData());
                if (padParams->mediaType == AVMEDIA_TYPE_VIDEO) {
                    demuxerOutPad = pad.second;
                    break;
                }
            }
        }

        auto decoder1InPad = decoder1->getInputPads().begin()->second;
        auto decoder1OutPad = decoder1->getOutputPads().begin()->second;
        auto decoder2InPad = decoder2->getInputPads().begin()->second;
        auto decoder2OutPad = decoder2->getOutputPads().begin()->second;
        //    auto decoder3InPad = decoder3->getInputPads().begin()->second;
        //    auto decoder3OutPad = decoder3->getOutputPads().begin()->second;
        auto encoder1InPad = encoder1->getInputPads().begin()->second;
        auto encoder1OutPad = encoder1->getOutputPads().begin()->second;
        auto encoder2InPad = encoder2->getInputPads().begin()->second;
        auto encoder2OutPad = encoder2->getOutputPads().begin()->second;
        auto renderer1InPad = renderer1->getInputPads().begin()->second;
        //    auto renderer2InPad = renderer2->getInputPads().begin()->second;
        auto ccInPad = cc->getInputPads().begin()->second;
        //    auto cc2InPad = cc2->getInputPads().begin()->second;
        auto yuvrgbconverterInPad = yuvrgbconverter->getInputPads().begin()->second;
        auto yuvrgbconverterOutPad = yuvrgbconverter->getOutputPads().begin()->second;
        //    auto frameSaverInPad = frameSaver->getInputPads().begin()->second;

        auto muxerInPad1Id = muxer->createStreamPad();
        auto muxerInPad1 = muxer->getInputPad(muxerInPad1Id);

        //        auto muxerInPad2Id = muxer->createStreamPad();
        //        auto muxerInPad2 = muxer->getInputPad(muxerInPad2Id);

        //    cc2InPad->link(decoderOutPad);
        //    decoder1InPad->link(transcoderPacketOutPad);
        //    encoderInPad->link(decoder1OutPad);
        //    decoder2InPad->link(encoderOutPad);
        decoder1InPad->link(demuxerOutPad);
        //        ccInPad->link(decoder1OutPad);
        encoder1InPad->link(decoder1OutPad);
        //        encoder2InPad->link(decoder1OutPad);
        yuvrgbconverterInPad->link(decoder1OutPad);
        //    renderer2InPad->link(decoder1OutPad);
        renderer1InPad->link(yuvrgbconverterOutPad);
        //    renderer1InPad->link(decoder1OutPad);
        //    renderer2InPad->link(decoder2OutPad);
        //    yuvrgbconverterOutPad->link(frameSaverInPad);

        muxerInPad1->link(encoder1OutPad);
        //        muxerInPad2->link(encoder2OutPad);

        demuxer->open();

        renderer1->resize(1280, 720);
        //        renderer2->resize(1280, 720);

        demuxer->start();

        //        QTimer::singleShot(5000, [demuxer]() {
        //            QApplication::quit();
        //        });

        //    QTimer::singleShot(4000, [demuxer]{
        //        demuxer->pause(true);
        //        QTimer::singleShot(4000, [demuxer]{
        //            demuxer->pause(false);
        //    QTimer::singleShot(15000, [renderer1, renderer2] {
        //        renderer1->close();
        //        renderer2->close();
        //    });
        //        });
        //    });

        //    AVQt::api::IDesktopCaptureImpl::Config config{};
        //    config.fps = 1;
        //    config.sourceClass = AVQt::api::IDesktopCaptureImpl::SourceClass::Screen;
        //    auto capturer = std::make_shared<AVQt::DesktopCapturer>(config, registry);
        //    auto cc = std::make_shared<CommandConsumer>(registry);
        //
        //    capturer->init();
        //    cc->init();
        //
        //    auto ccInPad = cc->getInputPads().begin()->second;
        //    auto capturerOutPad = capturer->getOutputPads().begin()->second;
        //
        //    ccInPad->link(capturerOutPad);
        //
        //    capturer->open();
        //    capturer->start();
        //
        QObject::connect(app, &QApplication::aboutToQuit, [demuxer /*, buffer, &muxer, &decoder1, &renderer1, &encoder*/] {
            demuxer->close();
            //        demuxer.reset();
            //        decoder1.reset();
            //        renderer1.reset();
            //        encoder.reset();
            //        muxer.reset();
            //        std::cout << "Buffer size: " << buffer->size() << std::endl;
            //        QFile output("output.ts");
            //        output.open(QIODevice::WriteOnly);
            //        output.write(buffer->data(), buffer->size());
            //        output.close();
            //        delete buffer;
        });
        //
        //    QTimer::singleShot(15000, [] {
        //        QApplication::quit();
        //    });
    }

    int ret = QApplication::exec();
    demuxer.reset();
    decoder1.reset();
    decoder2.reset();
    //    decoder3.reset();
    encoder1.reset();
    encoder2.reset();
    renderer1.reset();
    //    renderer2.reset();
    muxer.reset();

    return ret;
}
