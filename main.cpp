#include "AVQt"

#include <QApplication>
#include <csignal>

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

//    auto inputFile = new QFile(QFileDialog::getOpenFileName(nullptr, "Select video file",
//                                                  QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0],
//                                                  "Video file (*.mkv *.mp4 *.webm *.m4v *.ts)"));
//
//    inputFile->open(QIODevice::ReadOnly);
//
//    auto decoder = new AVQt::DecoderVAAPI(inputFile);
//
//    auto renderer = new AVQt::OpenGLWidgetRenderer;
//
//    decoder->registerCallback(renderer, AVQt::IFrameSource::CB_AVFRAME);
//
//    renderer->init();
//    decoder->init();
//
//    renderer->setBaseSize(1280, 720);
//    renderer->showNormal();
//
//    renderer->start();
//    decoder->start();
//
//    QObject::connect(app, &QApplication::aboutToQuit, [&] {
//        decoder->stop();
//        renderer->stop();
//        decoder->deinit();
//        renderer->deinit();
//
//        decoder->unregisterCallback(renderer);
//
//        delete renderer;
//        delete decoder;
//    });

    AVQt::OpenGLRenderer w;
    w.setMinimumSize(QSize(640, 360));
    QFile file(QFileDialog::getOpenFileName(nullptr,
                                            "Select video file",
                                            QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0],
                                            "Video files (*.mp4 *.mkv *.m4v *.ts *.webm)"));
    if (file.fileName().isEmpty()) {
        exit(0);
    }
    file.open(QIODevice::ReadOnly);
    AVQt::DecoderVAAPI decoder(&file);
    decoder.registerCallback(&w, AVQt::IFrameSource::CB_AVFRAME);
    decoder.init();
//    QWidget *widget = QWidget::createWindowContainer(&w);
    w.showNormal();
    w.requestActivate();
    w.start();
    decoder.start();
//    AVFrame *avFrame = av_frame_alloc();
//    avFrame->width = 1920;
//    avFrame->height = 1080;
//    auto file = QFile(":/images/frame.yuv");
//    file.open(QIODevice::ReadOnly);
//    auto data = file.readAll();
////    std::reverse(data.begin(), data.end());
//    av_image_fill_arrays(avFrame->data, avFrame->linesize, reinterpret_cast<const uint8_t *>(data.constData()), AV_PIX_FMT_NV12, 1920, 1080, 1);
////    avFrame->data[0] = reinterpret_cast<uint8_t *>(data.first(data.size() / 2).data());
////    avFrame->data[1] = reinterpret_cast<uint8_t *>(data.last(data.size() / 2).data());
////    avFrame->linesize[0] = data.size() / 2 / 1080;
////    avFrame->linesize[1] = data.size() / 2 / 1080;
//    avFrame->format = AV_PIX_FMT_NV12;
//    w.onFrame(avFrame, av_make_q(0, 0), av_make_q(0, 0), 1000);

    QObject::connect(app, &QApplication::aboutToQuit, [&] {
        w.stop();
        decoder.stop();
        decoder.deinit();
    });

    return QApplication::exec();
}