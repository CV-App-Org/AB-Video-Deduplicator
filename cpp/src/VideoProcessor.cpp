#include "VideoProcessor.h"

#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>

VideoProcessor::VideoProcessor(const QString &videoA,
                               const QString &videoB,
                               const QString &output,
                               int fps,
                               const QString &tempDir,
                               bool useGpu,
                               QObject *parent)
    : QThread(parent),
      m_videoA(videoA),
      m_videoB(videoB),
      m_output(output),
      m_fps(fps),
      m_tempDir(tempDir),
      m_useGpu(useGpu)
{
}

static QString resolveTool(const QString &base)
{
    // Prefer a binary that ships alongside the executable.
    const QString dir = QCoreApplication::applicationDirPath();
    QStringList candidates;
#ifdef Q_OS_WIN
    candidates << dir + "/" + base + ".exe";
#else
    candidates << dir + "/" + base;
#endif
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    // Fall back to PATH.
    return base;
}

QString VideoProcessor::ffmpegPath() { return resolveTool("ffmpeg"); }
QString VideoProcessor::ffprobePath() { return resolveTool("ffprobe"); }

QSet<long long> VideoProcessor::getAPositions(int fps, long long nA)
{
    QSet<long long> positions;
    if (fps == 60) {
        for (long long m = 0; m < nA; ++m)
            positions.insert(m <= 2 ? m : 2 + 2 * (m - 2));
    } else if (fps == 120) {
        for (long long m = 0; m < nA; ++m)
            positions.insert(m <= 1 ? m : 1 + 4 * (m - 1));
    } else if (fps == 240) {
        if (nA == 0)
            return positions;
        if (nA <= 2) {
            for (long long m = 0; m < nA; ++m)
                positions.insert(m);
            return positions;
        }
        positions.insert(0);
        positions.insert(1);
        long long nextPos = 1;
        const int intervals[3] = {8, 9, 7};
        for (long long i = 2; i < nA; ++i) {
            nextPos += intervals[(i - 2) % 3];
            positions.insert(nextPos);
        }
    }
    return positions;
}

VideoProcessor::VideoInfo VideoProcessor::getVideoInfo(const QString &path, QString *err)
{
    VideoInfo info;
    QProcess probe;
    QStringList args;
    args << "-v" << "quiet" << "-print_format" << "json"
         << "-show_streams" << "-show_format" << path;
    probe.start(ffprobePath(), args);
    if (!probe.waitForStarted(10000)) {
        if (err) *err = QString("无法启动 ffprobe: %1").arg(ffprobePath());
        return info;
    }
    probe.waitForFinished(-1);
    const QByteArray out = probe.readAllStandardOutput();
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(out, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = QString("解析 ffprobe 输出失败: %1").arg(path);
        return info;
    }
    QJsonObject root = doc.object();
    QJsonObject videoStream;
    bool found = false;
    for (const QJsonValue &v : root.value("streams").toArray()) {
        QJsonObject s = v.toObject();
        if (s.value("codec_type").toString() == "video") {
            videoStream = s;
            found = true;
            break;
        }
    }
    if (!found) {
        if (err) *err = QString("未找到视频流: %1").arg(path);
        return info;
    }
    info.width = videoStream.value("width").toInt();
    info.height = videoStream.value("height").toInt();

    // r_frame_rate is "num/den".
    const QString rate = videoStream.value("r_frame_rate").toString("0/1");
    const QStringList parts = rate.split('/');
    if (parts.size() == 2) {
        const double num = parts[0].toDouble();
        const double den = parts[1].toDouble();
        info.fps = den > 0 ? num / den : 0.0;
    } else {
        info.fps = rate.toDouble();
    }

    // Duration: stream first, then format.
    QString durStr = videoStream.value("duration").toString();
    if (durStr.isEmpty())
        durStr = root.value("format").toObject().value("duration").toString();
    info.duration = durStr.toDouble();

    // Frame count: nb_frames, else duration*fps.
    const QString nbStr = videoStream.value("nb_frames").toString();
    bool nbOk = false;
    long long nb = nbStr.toLongLong(&nbOk);
    if (nbOk && nb > 0)
        info.totalFrames = nb;
    else if (info.duration > 0 && info.fps > 0)
        info.totalFrames = static_cast<long long>(info.duration * info.fps);

    if (info.fps <= 0 || info.totalFrames <= 0 || info.duration <= 0) {
        if (err) *err = QString("视频元数据不完整或无效: %1").arg(path);
        return info;
    }
    info.valid = true;
    return info;
}

bool VideoProcessor::resizeVideo(const QString &in, const QString &out, int w, int h, QString *err)
{
    if (!QFileInfo::exists(in)) {
        if (err) *err = QString("输入视频文件 %1 不存在！").arg(in);
        return false;
    }
    const QString encoder = m_useGpu ? "h264_nvenc" : "libx264";
    QStringList args;
    args << "-y" << "-i" << in
         << "-vf" << QString("scale=%1:%2:force_original_aspect_ratio=decrease,"
                             "pad=%1:%2:(ow-iw)/2:(oh-ih)/2").arg(w).arg(h)
         << "-c:v" << encoder;
    if (m_useGpu)
        args << "-preset" << "p6";
    else
        args << "-crf" << "23";
    args << "-c:a" << "aac" << "-b:a" << "128k" << out;

    QProcess p;
    p.start(ffmpegPath(), args);
    if (!p.waitForStarted(10000)) {
        if (err) *err = "无法启动 ffmpeg (resize)";
        return false;
    }
    p.waitForFinished(-1);
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0 || !QFileInfo::exists(out)) {
        if (err) *err = QString("FFmpeg 调整尺寸失败:\n%1").arg(QString::fromUtf8(p.readAllStandardError()));
        return false;
    }
    return true;
}

QVector<QByteArray> VideoProcessor::readAllFrames(const QString &path, int w, int h, QString *err)
{
    QVector<QByteArray> frames;
    const int frameSize = w * h * 3;
    QProcess p;
    QStringList args;
    args << "-i" << path << "-f" << "image2pipe"
         << "-pix_fmt" << "bgr24" << "-vcodec" << "rawvideo" << "-";
    p.start(ffmpegPath(), args);
    if (!p.waitForStarted(10000)) {
        if (err) *err = "无法启动 ffmpeg (帧读取)";
        return frames;
    }

    QByteArray buffer;
    // Drain stdout while ffmpeg runs to avoid pipe deadlock.
    while (p.state() != QProcess::NotRunning) {
        if (p.waitForReadyRead(100))
            buffer += p.readAllStandardOutput();
    }
    buffer += p.readAllStandardOutput();
    p.waitForFinished(-1);

    if (p.exitCode() != 0 && buffer.isEmpty()) {
        if (err) *err = QString("FFmpeg 帧读取失败:\n%1").arg(QString::fromUtf8(p.readAllStandardError()));
        return frames;
    }

    const long long n = buffer.size() / frameSize;
    frames.reserve(static_cast<int>(n));
    for (long long i = 0; i < n; ++i)
        frames.append(buffer.mid(static_cast<int>(i * frameSize), frameSize));
    return frames;
}

void VideoProcessor::run()
{
    QElapsedTimer timer;
    timer.start();
    auto elapsed = [&timer]() { return timer.elapsed() / 1000.0; };

    const QString tempB = QDir(m_tempDir).filePath("resized_b.mp4");
    const QString tempOutput = QDir(m_tempDir).filePath("temp_output.mp4");
    QStringList toClean;
    toClean << tempOutput;
    QString pathBtoProcess = m_videoB;

    QString err;
    try {
        QDir().mkpath(m_tempDir);
        emit status(QString("开始处理，检查视频信息... (t=%1s)").arg(elapsed(), 0, 'f', 2));
        emit progress(5);

        VideoInfo a = getVideoInfo(m_videoA, &err);
        if (!a.valid) throw std::runtime_error(err.toStdString());
        emit status(QString("视频A信息: %1x%2, %3fps, %4s, %5帧")
                    .arg(a.width).arg(a.height)
                    .arg(a.fps, 0, 'f', 2).arg(a.duration, 0, 'f', 2).arg(a.totalFrames));

        VideoInfo b = getVideoInfo(m_videoB, &err);
        if (!b.valid) throw std::runtime_error(err.toStdString());
        emit status(QString("视频B信息: %1x%2").arg(b.width).arg(b.height));

        if (a.duration <= 0)
            throw std::runtime_error("无法获取视频A的有效时长，处理中止。");

        if (a.width != b.width || a.height != b.height) {
            emit status(QString("分辨率不一致，将视频B (%1x%2) 调整为视频A的尺寸 (%3x%4)... (t=%5s)")
                        .arg(b.width).arg(b.height).arg(a.width).arg(a.height)
                        .arg(elapsed(), 0, 'f', 2));
            if (!resizeVideo(m_videoB, tempB, a.width, a.height, &err))
                throw std::runtime_error(err.toStdString());
            pathBtoProcess = tempB;
            toClean << tempB;
        } else {
            emit status("分辨率一致，跳过尺寸调整。");
        }
        emit progress(10);

        const long long totalFramesC = static_cast<long long>(a.duration * m_fps);
        emit status(QString("目标视频C: %1fps, 时长与A一致(%2s), 总帧数: %3")
                    .arg(m_fps).arg(a.duration, 0, 'f', 2).arg(totalFramesC));
        emit status(QString("准备帧序列混合... (t=%1s)").arg(elapsed(), 0, 'f', 2));

        const QSet<long long> positionsA = getAPositions(m_fps, a.totalFrames);

        // Load frames into memory (B is cycled, matching the Python itertools.cycle).
        QVector<QByteArray> framesA = readAllFrames(m_videoA, a.width, a.height, &err);
        if (framesA.isEmpty()) throw std::runtime_error(err.isEmpty() ? "视频A无可用帧" : err.toStdString());
        QVector<QByteArray> framesB = readAllFrames(pathBtoProcess, a.width, a.height, &err);
        if (framesB.isEmpty()) throw std::runtime_error(err.isEmpty() ? "视频B无可用帧" : err.toStdString());

        const QString encoder = m_useGpu ? "h264_nvenc" : "libx264";
        QStringList writerArgs;
        writerArgs << "-y" << "-f" << "rawvideo" << "-vcodec" << "rawvideo"
                   << "-pix_fmt" << "bgr24"
                   << "-s" << QString("%1x%2").arg(a.width).arg(a.height)
                   << "-r" << QString::number(m_fps)
                   << "-i" << "-" << "-c:v" << encoder;
        if (m_useGpu)
            writerArgs << "-preset" << "p6";
        else
            writerArgs << "-crf" << "23";
        writerArgs << "-pix_fmt" << "yuv420p" << tempOutput;

        QProcess writer;
        writer.start(ffmpegPath(), writerArgs);
        if (!writer.waitForStarted(10000))
            throw std::runtime_error("无法启动 ffmpeg (写入)");

        emit status(QString("开始混合帧... (t=%1s)").arg(elapsed(), 0, 'f', 2));
        emit progress(20);

        long long aCounter = 0;
        long long bIndex = 0;
        for (long long i = 0; i < totalFramesC; ++i) {
            const QByteArray *frame;
            if (positionsA.contains(i) && aCounter < a.totalFrames && aCounter < framesA.size()) {
                frame = &framesA[static_cast<int>(aCounter)];
                ++aCounter;
            } else {
                frame = &framesB[static_cast<int>(bIndex)];
                bIndex = (bIndex + 1) % framesB.size();
            }
            writer.write(*frame);
            // Backpressure: don't let the write buffer grow unbounded.
            while (writer.bytesToWrite() > 8 * frame->size())
                writer.waitForBytesWritten(-1);

            if ((i + 1) % 50 == 0 || (i + 1) == totalFramesC) {
                int prog = 20 + static_cast<int>(70.0 * (i + 1) / totalFramesC);
                emit progress(qMin(prog, 89));
                emit status(QString("处理帧: %1 / %2 (t=%3s)")
                            .arg(i + 1).arg(totalFramesC).arg(elapsed(), 0, 'f', 2));
            }
        }

        emit status(QString("混合完成，正在生成最终视频文件... (t=%1s)").arg(elapsed(), 0, 'f', 2));
        writer.closeWriteChannel();
        writer.waitForFinished(-1);
        if (writer.exitStatus() != QProcess::NormalExit || writer.exitCode() != 0)
            throw std::runtime_error(QString("FFmpeg写入视频失败: %1")
                                     .arg(QString::fromUtf8(writer.readAllStandardError())).toStdString());
        emit progress(90);

        emit status(QString("合并音频... (t=%1s)").arg(elapsed(), 0, 'f', 2));
        QStringList muxArgs;
        muxArgs << "-y" << "-i" << tempOutput << "-i" << m_videoA
                << "-c:v" << "copy" << "-c:a" << "aac" << "-b:a" << "128k"
                << "-shortest" << m_output;
        QProcess mux;
        mux.start(ffmpegPath(), muxArgs);
        if (!mux.waitForStarted(10000))
            throw std::runtime_error("无法启动 ffmpeg (合并音频)");
        mux.waitForFinished(-1);
        if (mux.exitStatus() != QProcess::NormalExit || mux.exitCode() != 0 || !QFileInfo::exists(m_output))
            throw std::runtime_error(QString("FFmpeg合并音频失败: %1")
                                     .arg(QString::fromUtf8(mux.readAllStandardError())).toStdString());

        emit progress(100);
        emit status(QString("视频处理完成! (总耗时: %1s)").arg(elapsed(), 0, 'f', 2));
        emit finishedOk();
    } catch (const std::exception &e) {
        emit error(QString("错误：%1").arg(QString::fromUtf8(e.what())));
    }

    for (const QString &f : toClean) {
        if (QFileInfo::exists(f))
            QFile::remove(f);
    }
}
