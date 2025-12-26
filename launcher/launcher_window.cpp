#include "launcher_window.h"
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>

LauncherWindow::LauncherWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_logTextEdit(nullptr)
    , m_closeButton(nullptr)
    , m_buildProcess(nullptr)
    , m_isBuilding(false)
{
    setWindowTitle("API检测器 - 启动器");
    setFixedSize(800, 600);

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    m_statusLabel = new QLabel("正在检查程序状态...", m_centralWidget);
    m_statusLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #333; }");
    mainLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(m_centralWidget);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  border: 2px solid grey;"
        "  border-radius: 5px;"
        "  text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #05B8CC;"
        "  width: 20px;"
        "}"
    );
    mainLayout->addWidget(m_progressBar);

    m_logTextEdit = new QTextEdit(m_centralWidget);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  font-family: Consolas, Monaco, monospace;"
        "  font-size: 10px;"
        "  border: 1px solid #3e3e3e;"
        "}"
    );
    mainLayout->addWidget(m_logTextEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_closeButton = new QPushButton("关闭", m_centralWidget);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #d9534f;"
        "  color: white;"
        "  border: none;"
        "  padding: 8px 16px;"
        "  border-radius: 4px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #c9302c;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #ac2925;"
        "}"
    );
    connect(m_closeButton, &QPushButton::clicked, this, &LauncherWindow::onCloseClicked);
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);

    m_buildProcess = new QProcess(this);
    connect(m_buildProcess, &QProcess::readyReadStandardOutput, this, &LauncherWindow::onBuildOutput);
    connect(m_buildProcess, &QProcess::readyReadStandardError, this, &LauncherWindow::onBuildOutput);
    connect(m_buildProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LauncherWindow::onBuildFinished);
    connect(m_buildProcess, &QProcess::errorOccurred, this, &LauncherWindow::onBuildError);
}

LauncherWindow::~LauncherWindow()
{
    if (m_buildProcess && m_buildProcess->state() != QProcess::NotRunning) {
        m_buildProcess->kill();
        m_buildProcess->waitForFinished();
    }
}

void LauncherWindow::checkAndLaunch()
{
    appendLog("========================================", "#888888");
    appendLog("API检测器启动器 v1.0.0", "#05B8CC");
    appendLog("========================================", "#888888");
    appendLog("");

    if (!areDependenciesInstalled()) {
        appendLog("✗ 依赖未安装，开始自动安装...", "#dc3545");
        m_statusLabel->setText("正在安装依赖...");
        m_isBuilding = true;
        installDependencies();
        return;
    }

    if (isGuiBuilt()) {
        appendLog("✓ 检测到GUI程序已构建", "#28a745");
        m_statusLabel->setText("正在启动GUI程序...");
        m_progressBar->setValue(100);

        QTimer::singleShot(1000, this, [this]() {
            startGui();
        });
    } else {
        appendLog("✗ GUI程序未构建，开始自动构建...", "#dc3545");
        m_statusLabel->setText("正在自动构建GUI程序...");
        m_isBuilding = true;
        startBuild();
    }
}

bool LauncherWindow::isGuiBuilt()
{
    QString guiPath = QDir::currentPath() + "/build-gui/Release/api-checker-gui.exe";
    if (!QFile::exists(guiPath)) {
        guiPath = QDir::currentPath() + "/build-gui/api-checker-gui.exe";
    }
    if (!QFile::exists(guiPath)) {
        guiPath = QDir::currentPath() + "/api-checker-gui.exe";
    }
    return QFile::exists(guiPath);
}

bool LauncherWindow::areDependenciesInstalled()
{
    QString depsZip = QDir::currentPath() + "/dependencies.zip";
    if (!QFile::exists(depsZip)) {
        return true;
    }

    QStringList requiredDlls;
    requiredDlls << "Qt6Core.dll" << "Qt6Widgets.dll" << "Qt6Network.dll";

    for (const QString &dll : requiredDlls) {
        if (!QFile::exists(dll)) {
            return false;
        }
    }

    return true;
}

void LauncherWindow::installDependencies()
{
    appendLog("开始安装依赖...", "#05B8CC");
    appendLog("工作目录: " + QDir::currentPath(), "#888888");
    appendLog("");

    QString depsZip = QDir::currentPath() + "/dependencies.zip";

    if (!QFile::exists(depsZip)) {
        appendLog("错误：找不到依赖包 dependencies.zip", "#dc3545");
        appendLog("依赖应该已经安装，继续启动...", "#888888");
        m_isBuilding = false;
        checkAndLaunch();
        return;
    }

    appendLog("找到依赖包: dependencies.zip", "#05B8CC");
    appendLog("开始解压依赖...", "#05B8CC");
    appendLog("========================================", "#888888");
    appendLog("");

    m_progressBar->setRange(0, 0);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  border: 2px solid grey;"
        "  border-radius: 5px;"
        "  text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #f39c12;"
        "  width: 20px;"
        "}"
    );

    QString powershellCommand = QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
        .arg(depsZip)
        .arg(QDir::currentPath());

    m_buildProcess->start("powershell.exe", QStringList() << "-Command" << powershellCommand);

    if (!m_buildProcess->waitForStarted()) {
        appendLog("错误：无法启动解压进程", "#dc3545");
        QMessageBox::critical(this, "错误", "无法启动解压进程。");
        m_isBuilding = false;
        return;
    }

    appendLog("✓ 解压进程已启动", "#28a745");
}

void LauncherWindow::startGui()
{
    appendLog("正在启动GUI程序...", "#05B8CC");

    QString guiPath;
    if (QFile::exists("build-gui/Release/api-checker-gui.exe")) {
        guiPath = "build-gui/Release/api-checker-gui.exe";
    } else if (QFile::exists("build-gui/api-checker-gui.exe")) {
        guiPath = "build-gui/api-checker-gui.exe";
    } else if (QFile::exists("api-checker-gui.exe")) {
        guiPath = "api-checker-gui.exe";
    }

    if (guiPath.isEmpty()) {
        appendLog("错误：找不到GUI程序", "#dc3545");
        QMessageBox::critical(this, "错误", "找不到GUI程序，请手动构建。");
        return;
    }

    appendLog("启动路径: " + guiPath, "#888888");

    if (QProcess::startDetached(guiPath)) {
        appendLog("✓ GUI程序启动成功！", "#28a745");
        m_statusLabel->setText("GUI程序已启动");
        appendLog("启动器将在3秒后自动关闭...", "#888888");

        QTimer::singleShot(3000, this, [this]() {
            QApplication::quit();
        });
    } else {
        appendLog("✗ GUI程序启动失败", "#dc3545");
        QMessageBox::critical(this, "错误", "无法启动GUI程序。");
    }
}

void LauncherWindow::startBuild()
{
    appendLog("开始构建过程...", "#05B8CC");
    appendLog("工作目录: " + QDir::currentPath(), "#888888");
    appendLog("");

    QString buildScript = QDir::currentPath() + "/build_and_package.bat";

    if (!QFile::exists(buildScript)) {
        appendLog("错误：找不到构建脚本 build_and_package.bat", "#dc3545");
        QMessageBox::critical(this, "错误", "找不到构建脚本。");
        return;
    }

    appendLog("执行构建脚本: build_and_package.bat", "#05B8CC");
    appendLog("========================================", "#888888");
    appendLog("");

    m_buildProcess->start("cmd.exe", QStringList() << "/c" << buildScript);

    if (!m_buildProcess->waitForStarted()) {
        appendLog("错误：无法启动构建进程", "#dc3545");
        QMessageBox::critical(this, "错误", "无法启动构建进程。");
        return;
    }

    appendLog("✓ 构建进程已启动", "#28a745");
    m_progressBar->setRange(0, 0);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  border: 2px solid grey;"
        "  border-radius: 5px;"
        "  text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #f39c12;"
        "  width: 20px;"
        "}"
    );
}

void LauncherWindow::onBuildOutput()
{
    QByteArray output = m_buildProcess->readAllStandardOutput();
    QByteArray error = m_buildProcess->readAllStandardError();

    if (!output.isEmpty()) {
        QString outputStr = QString::fromLocal8Bit(output);
        appendLog(outputStr, "#d4d4d4");
    }

    if (!error.isEmpty()) {
        QString errorStr = QString::fromLocal8Bit(error);
        appendLog(errorStr, "#dc3545");
    }
}

void LauncherWindow::onBuildFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    appendLog("");
    appendLog("========================================", "#888888");

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        QString depsZip = QDir::currentPath() + "/dependencies.zip";

        if (QFile::exists(depsZip)) {
            appendLog("✓ 依赖安装完成！", "#28a745");
            m_statusLabel->setText("依赖安装完成，正在清理...");
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(100);
            m_progressBar->setStyleSheet(
                "QProgressBar {"
                "  border: 2px solid grey;"
                "  border-radius: 5px;"
                "  text-align: center;"
                "}"
                "QProgressBar::chunk {"
                "  background-color: #28a745;"
                "  width: 20px;"
                "}"
            );

            appendLog("删除依赖包...", "#888888");
            QFile::remove(depsZip);

            m_isBuilding = false;

            QTimer::singleShot(1000, this, [this]() {
                checkAndLaunch();
            });
        } else {
            appendLog("✓ 构建完成！", "#28a745");
            m_statusLabel->setText("构建完成，正在启动GUI...");
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(100);
            m_progressBar->setStyleSheet(
                "QProgressBar {"
                "  border: 2px solid grey;"
                "  border-radius: 5px;"
                "  text-align: center;"
                "}"
                "QProgressBar::chunk {"
                "  background-color: #28a745;"
                "  width: 20px;"
                "}"
            );

            QTimer::singleShot(2000, this, [this]() {
                startGui();
            });
        }
    } else {
        QString depsZip = QDir::currentPath() + "/dependencies.zip";

        if (QFile::exists(depsZip)) {
            appendLog("✗ 依赖安装失败，退出码: " + QString::number(exitCode), "#dc3545");
            m_statusLabel->setText("依赖安装失败");
        } else {
            appendLog("✗ 构建失败，退出码: " + QString::number(exitCode), "#dc3545");
            m_statusLabel->setText("构建失败");
        }

        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_progressBar->setStyleSheet(
            "QProgressBar {"
            "  border: 2px solid grey;"
            "  border-radius: 5px;"
            "  text-align: center;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: #dc3545;"
            "  width: 20px;"
            "}"
        );
        QMessageBox::critical(this, "操作失败",
            "操作失败，请查看日志了解详情。\n"
            "您可以尝试手动运行 build_and_package.bat 进行构建。");
    }

    m_isBuilding = false;
}

void LauncherWindow::onBuildError(QProcess::ProcessError error)
{
    appendLog("构建进程错误: " + m_buildProcess->errorString(), "#dc3545");
    m_statusLabel->setText("构建出错");
    m_isBuilding = false;
}

void LauncherWindow::onCloseClicked()
{
    if (m_isBuilding) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "确认关闭",
            "构建正在进行中，确定要关闭吗？",
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            if (m_buildProcess && m_buildProcess->state() != QProcess::NotRunning) {
                m_buildProcess->kill();
                m_buildProcess->waitForFinished();
            }
            QApplication::quit();
        }
    } else {
        QApplication::quit();
    }
}

void LauncherWindow::appendLog(const QString &message, const QString &color)
{
    if (message.isEmpty()) return;

    QStringList lines = message.split('\n');
    for (const QString &line : lines) {
        if (line.isEmpty()) {
            m_logTextEdit->append("");
        } else {
            m_logTextEdit->append(QString("<span style='color:%1;'>%2</span>").arg(color).arg(line.toHtmlEscaped()));
        }
    }

    m_logTextEdit->verticalScrollBar()->setValue(m_logTextEdit->verticalScrollBar()->maximum());
}
