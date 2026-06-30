#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

class QLabel;
class QPushButton;
class QRadioButton;
class QCheckBox;
class QProgressBar;
class QTextEdit;
class VideoProcessor;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void selectVideoA();
    void selectVideoB();
    void selectOutput();
    void runProcessing();
    void onProgress(int value);
    void onStatus(const QString &text);
    void onFinished();
    void onError(const QString &message);

private:
    void checkRunEnable();
    void setControlsEnabled(bool enabled);
    void appendText(const QString &text);

    QLabel *m_labelA;
    QLabel *m_labelB;
    QLabel *m_labelOutput;
    QPushButton *m_btnA;
    QPushButton *m_btnB;
    QPushButton *m_btnOutput;
    QPushButton *m_btnRun;
    QRadioButton *m_radio60;
    QRadioButton *m_radio120;
    QRadioButton *m_radio240;
    QCheckBox *m_gpuCheckbox;
    QProgressBar *m_progressBar;
    QTextEdit *m_textOutput;

    QString m_videoAPath;
    QString m_videoBPath;
    QString m_outputPath;
    QString m_tempDir;
    VideoProcessor *m_processor = nullptr;
};

#endif // MAINWINDOW_H
