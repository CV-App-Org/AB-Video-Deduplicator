#include <QApplication>
#include <QCoreApplication>
#include <QStringList>
#include <QDir>
#include <cstdio>

#include "MainWindow.h"
#include "VideoProcessor.h"

extern const char *mainWindowStyleSheet();

static void printUsage(const char *prog)
{
    std::fprintf(stderr,
        "用法:\n"
        "  GUI 模式:  %s\n"
        "  CLI 模式:  %s --cli <videoA> <videoB> <output> <fps:60|120|240> [--gpu]\n",
        prog, prog);
}

static int runCli(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    // args: [prog, --cli, A, B, out, fps, (--gpu)]
    if (args.size() < 6) {
        printUsage(argv[0]);
        return 2;
    }
    const QString a = args.at(2);
    const QString b = args.at(3);
    const QString out = args.at(4);
    const int fps = args.at(5).toInt();
    const bool gpu = args.contains("--gpu");
    if (fps != 60 && fps != 120 && fps != 240) {
        std::fprintf(stderr, "fps 必须是 60 / 120 / 240\n");
        return 2;
    }

    const QString tempDir = QDir(QDir::tempPath()).filePath("ab_dedup_cpp_cli");

    VideoProcessor proc(a, b, out, fps, tempDir, gpu);
    int exitCode = 0;
    QObject::connect(&proc, &VideoProcessor::status, [](const QString &s) {
        std::fprintf(stdout, "STATUS: %s\n", s.toUtf8().constData());
        std::fflush(stdout);
    });
    QObject::connect(&proc, &VideoProcessor::progress, [](int p) {
        std::fprintf(stdout, "PROGRESS: %d\n", p);
        std::fflush(stdout);
    });
    QObject::connect(&proc, &VideoProcessor::error, [&](const QString &e) {
        std::fprintf(stderr, "ERROR: %s\n", e.toUtf8().constData());
        exitCode = 1;
        app.quit();
    });
    QObject::connect(&proc, &VideoProcessor::finishedOk, [&]() {
        std::fprintf(stdout, "DONE\n");
        exitCode = 0;
        app.quit();
    });
    proc.start();
    app.exec();
    proc.wait();
    return exitCode;
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == "--cli")
            return runCli(argc, argv);
    }
    QApplication app(argc, argv);
    app.setStyleSheet(QString::fromUtf8(mainWindowStyleSheet()));
    MainWindow w;
    w.show();
    return app.exec();
}
