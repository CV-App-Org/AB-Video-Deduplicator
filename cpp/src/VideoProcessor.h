#ifndef VIDEOPROCESSOR_H
#define VIDEOPROCESSOR_H

#include <QThread>
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QSet>

// Core processing engine. Mirrors the Python VideoProcessor:
// blends frames from video A (content) and video B (material) into a
// high-frame-rate output to change the file's data fingerprint.
// All video work is delegated to the external ffmpeg / ffprobe binaries.
class VideoProcessor : public QThread
{
    Q_OBJECT
public:
    struct VideoInfo {
        int width = 0;
        int height = 0;
        double fps = 0.0;
        double duration = 0.0;
        long long totalFrames = 0;
        bool valid = false;
    };

    VideoProcessor(const QString &videoA,
                   const QString &videoB,
                   const QString &output,
                   int fps,
                   const QString &tempDir,
                   bool useGpu,
                   QObject *parent = nullptr);

    // Locate ffmpeg/ffprobe: prefer a binary shipped next to the app
    // (e.g. ffmpeg.exe on Windows), otherwise fall back to PATH.
    static QString ffmpegPath();
    static QString ffprobePath();

    // Frame positions where video A frames are inserted, by target fps.
    static QSet<long long> getAPositions(int fps, long long nA);

signals:
    void progress(int value);
    void status(const QString &message);
    void finishedOk();
    void error(const QString &message);

protected:
    void run() override;

private:
    VideoInfo getVideoInfo(const QString &path, QString *err);
    bool resizeVideo(const QString &in, const QString &out, int w, int h, QString *err);
    QVector<QByteArray> readAllFrames(const QString &path, int w, int h, QString *err);

    QString m_videoA;
    QString m_videoB;
    QString m_output;
    int m_fps;
    QString m_tempDir;
    bool m_useGpu;
};

#endif // VIDEOPROCESSOR_H
