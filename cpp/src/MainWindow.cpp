#include "MainWindow.h"
#include "VideoProcessor.h"

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFileDialog>
#include <QScrollBar>
#include <QDir>
#include <QStandardPaths>

static const char *kQss = R"(
QWidget {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1e1e2f, stop:1 #141422);
    color: #e0e0e0;
}
QFrame {
    background: rgba(40, 40, 60, 0.9);
    border: none;
    border-radius: 10px;
    padding: 15px;
}
QLabel#section_title { font-size: 14px; font-weight: 600; color: #ffffff; }
QLabel#path_label {
    background: rgba(60, 60, 80, 0.8);
    border: 1px solid #555;
    border-radius: 5px;
    padding: 8px;
    font-size: 14px;
    color: #e0e0e0;
}
QPushButton#select_button {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4a90e2, stop:1 #357abd);
    color: white; border: none; padding: 8px 15px; font-size: 14px; border-radius: 5px;
}
QPushButton#select_button:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #5aa1f2, stop:1 #4688d1);
}
QRadioButton, QCheckBox { font-size: 14px; color: #e0e0e0; }
QRadioButton::indicator, QCheckBox::indicator {
    width: 20px; height: 20px; border-radius: 5px; border: 2px solid #ffd700; background: #2a2a3a;
}
QRadioButton::indicator { border-radius: 10px; }
QRadioButton::indicator:checked, QCheckBox::indicator:checked { background: #ffd700; border: 2px solid #ffd700; }
QRadioButton::indicator:hover, QCheckBox::indicator:hover { border: 2px solid #ffea00; }
QPushButton#run_button {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff7e5f, stop:1 #feb47b);
    color: white; border: none; padding: 12px 25px; font-size: 16px; font-weight: bold; border-radius: 8px;
}
QPushButton#run_button:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff926f, stop:1 #ffc48b);
}
QPushButton#run_button:disabled {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #999, stop:1 #777); color: #ccc;
}
QProgressBar {
    background: rgba(40, 40, 60, 0.8); border-radius: 5px; text-align: center; font-size: 14px; color: #ffffff;
}
QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4a90e2, stop:1 #357abd); border-radius: 5px;
}
QTextEdit {
    background: rgba(30, 30, 50, 0.9); border: 1px solid #555; border-radius: 5px; font-size: 12px; color: #d0d0d0;
}
QLabel { background: transparent; }
)";

static QFrame *makePathRow(const QString &title, QLabel **pathLabel, QPushButton **button)
{
    QFrame *frame = new QFrame();
    QHBoxLayout *layout = new QHBoxLayout();
    layout->setSpacing(10);
    QLabel *t = new QLabel(title);
    t->setObjectName("section_title");
    QLabel *path = new QLabel(QStringLiteral("未选择"));
    path->setObjectName("path_label");
    path->setWordWrap(true);
    QPushButton *btn = new QPushButton(QStringLiteral("选择"));
    btn->setObjectName("select_button");
    layout->addWidget(t);
    layout->addWidget(path, 1);
    layout->addWidget(btn);
    frame->setLayout(layout);
    *pathLabel = path;
    *button = btn;
    return frame;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("AB视频去重工具 (C++ 版)"));
    setGeometry(100, 100, 600, 850);

    QWidget *container = new QWidget();
    setCentralWidget(container);
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    mainLayout->addWidget(makePathRow(QStringLiteral("视频A路径（搬运）"), &m_labelA, &m_btnA));
    mainLayout->addWidget(makePathRow(QStringLiteral("视频B路径（原创）"), &m_labelB, &m_btnB));
    mainLayout->addWidget(makePathRow(QStringLiteral("输出路径"), &m_labelOutput, &m_btnOutput));

    connect(m_btnA, &QPushButton::clicked, this, &MainWindow::selectVideoA);
    connect(m_btnB, &QPushButton::clicked, this, &MainWindow::selectVideoB);
    connect(m_btnOutput, &QPushButton::clicked, this, &MainWindow::selectOutput);

    QFrame *optionsFrame = new QFrame();
    QVBoxLayout *optionsLayout = new QVBoxLayout();
    optionsLayout->setSpacing(15);
    QLabel *fpsTitle = new QLabel(QStringLiteral("去重强度"));
    fpsTitle->setObjectName("section_title");
    optionsLayout->addWidget(fpsTitle);

    m_radio60 = new QRadioButton(QStringLiteral("去重率50%（DY+TK）"));
    m_radio120 = new QRadioButton(QStringLiteral("去重率75%（仅TK）"));
    m_radio240 = new QRadioButton(QStringLiteral("去重率87.5%（仅TK）"));
    m_radio60->setChecked(true);
    QButtonGroup *group = new QButtonGroup(this);
    group->addButton(m_radio60);
    group->addButton(m_radio120);
    group->addButton(m_radio240);
    QHBoxLayout *fpsRow = new QHBoxLayout();
    fpsRow->addWidget(m_radio60);
    fpsRow->addWidget(m_radio120);
    fpsRow->addWidget(m_radio240);
    fpsRow->addStretch();
    optionsLayout->addLayout(fpsRow);

    QLabel *gpuTitle = new QLabel(QStringLiteral("性能选项"));
    gpuTitle->setObjectName("section_title");
    optionsLayout->addWidget(gpuTitle);
    m_gpuCheckbox = new QCheckBox(QStringLiteral("启用GPU加速（需要NVIDIA显卡和驱动）"));
    m_gpuCheckbox->setChecked(false);
    optionsLayout->addWidget(m_gpuCheckbox);
    optionsFrame->setLayout(optionsLayout);
    mainLayout->addWidget(optionsFrame);

    m_btnRun = new QPushButton(QStringLiteral("运行"));
    m_btnRun->setObjectName("run_button");
    m_btnRun->setMinimumWidth(200);
    m_btnRun->setEnabled(false);
    connect(m_btnRun, &QPushButton::clicked, this, &MainWindow::runProcessing);
    mainLayout->addWidget(m_btnRun, 0, Qt::AlignCenter);

    QFrame *progressFrame = new QFrame();
    QVBoxLayout *progressLayout = new QVBoxLayout();
    progressLayout->setSpacing(10);
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    progressLayout->addWidget(m_progressBar);
    m_textOutput = new QTextEdit();
    m_textOutput->setReadOnly(true);
    progressLayout->addWidget(m_textOutput);
    progressFrame->setLayout(progressLayout);
    mainLayout->addWidget(progressFrame);

    container->setLayout(mainLayout);

    m_tempDir = QDir(QDir::homePath()).filePath(".video_temp_optimized_cpp");
    QDir().mkpath(m_tempDir);
}

void MainWindow::selectVideoA()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择视频A"), QString(),
        QStringLiteral("视频文件 (*.mp4 *.avi *.mov)"));
    if (!path.isEmpty()) {
        m_videoAPath = path;
        m_labelA->setText(path);
        checkRunEnable();
    }
}

void MainWindow::selectVideoB()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择视频B"), QString(),
        QStringLiteral("视频文件 (*.mp4 *.avi *.mov)"));
    if (!path.isEmpty()) {
        m_videoBPath = path;
        m_labelB->setText(path);
        checkRunEnable();
    }
}

void MainWindow::selectOutput()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("选择输出路径"), QStringLiteral("C.mp4"),
        QStringLiteral("视频文件 (*.mp4)"));
    if (!path.isEmpty()) {
        m_outputPath = path;
        m_labelOutput->setText(path);
        checkRunEnable();
    }
}

void MainWindow::checkRunEnable()
{
    m_btnRun->setEnabled(!m_videoAPath.isEmpty() && !m_videoBPath.isEmpty() && !m_outputPath.isEmpty());
}

void MainWindow::setControlsEnabled(bool enabled)
{
    m_btnA->setEnabled(enabled);
    m_btnB->setEnabled(enabled);
    m_btnOutput->setEnabled(enabled);
    const bool ready = enabled && !m_videoAPath.isEmpty() && !m_videoBPath.isEmpty() && !m_outputPath.isEmpty();
    m_btnRun->setEnabled(ready);
    m_radio60->setEnabled(enabled);
    m_radio120->setEnabled(enabled);
    m_radio240->setEnabled(enabled);
    m_gpuCheckbox->setEnabled(enabled);
}

void MainWindow::appendText(const QString &text)
{
    m_textOutput->append(QStringLiteral("• ") + text);
    m_textOutput->verticalScrollBar()->setValue(m_textOutput->verticalScrollBar()->maximum());
}

void MainWindow::runProcessing()
{
    int fps = 60;
    if (m_radio120->isChecked()) fps = 120;
    else if (m_radio240->isChecked()) fps = 240;
    const bool useGpu = m_gpuCheckbox->isChecked();

    setControlsEnabled(false);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet(QString());
    m_textOutput->clear();
    appendText(useGpu ? QStringLiteral("已启用GPU加速模式。") : QStringLiteral("使用CPU模式处理。"));

    m_processor = new VideoProcessor(m_videoAPath, m_videoBPath, m_outputPath, fps, m_tempDir, useGpu, this);
    connect(m_processor, &VideoProcessor::progress, this, &MainWindow::onProgress);
    connect(m_processor, &VideoProcessor::status, this, &MainWindow::onStatus);
    connect(m_processor, &VideoProcessor::finishedOk, this, &MainWindow::onFinished);
    connect(m_processor, &VideoProcessor::error, this, &MainWindow::onError);
    connect(m_processor, &VideoProcessor::finished, m_processor, &QObject::deleteLater);
    m_processor->start();
}

void MainWindow::onProgress(int value) { m_progressBar->setValue(value); }
void MainWindow::onStatus(const QString &text) { appendText(text); }

void MainWindow::onFinished()
{
    setControlsEnabled(true);
    appendText(QStringLiteral("处理完成！"));
}

void MainWindow::onError(const QString &message)
{
    setControlsEnabled(true);
    m_textOutput->append(QStringLiteral("❌ ") + message);
    m_progressBar->setStyleSheet(
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 #e74c3c, stop:1 #c0392b); border-radius: 5px; }");
}

// Exposed for main.cpp to apply the global stylesheet.
const char *mainWindowStyleSheet() { return kQss; }
