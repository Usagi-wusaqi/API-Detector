#include "main_window.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    createMenuBar();
    createToolBar();
    createStatusBar();
    connectSignals();
    loadSettings();
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::setupUi()
{
    setWindowTitle("API检测器 v1.0.0");
    resize(1200, 800);
    setMinimumSize(900, 600);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setMovable(true);

    m_inputWidget = new ApiInputWidget(this);
    m_resultWidget = new ResultWidget(this);
    m_historyWidget = new HistoryWidget(this);
    m_settingsWidget = new SettingsWidget(this);

    m_tabWidget->addTab(m_inputWidget, "📝 API输入");
    m_tabWidget->addTab(m_resultWidget, "📊 检测结果");
    m_tabWidget->addTab(m_historyWidget, "📜 历史记录");
    m_tabWidget->addTab(m_settingsWidget, "⚙️ 设置");

    setCentralWidget(m_tabWidget);
}

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    QMenu *fileMenu = menuBar->addMenu("文件(&F)");
    m_newDetectionAction = fileMenu->addAction("新建检测(&N)");
    m_newDetectionAction->setShortcut(QKeySequence::New);
    m_newDetectionAction->setStatusTip("开始新的API检测");

    m_resumeDetectionAction = fileMenu->addAction("恢复检测(&R)");
    m_resumeDetectionAction->setShortcut(QKeySequence("Ctrl+R"));
    m_resumeDetectionAction->setStatusTip("恢复未完成的检测");

    fileMenu->addSeparator();

    m_exportResultsAction = fileMenu->addAction("导出结果(&E)");
    m_exportResultsAction->setShortcut(QKeySequence("Ctrl+E"));
    m_exportResultsAction->setStatusTip("导出检测结果");

    fileMenu->addSeparator();

    m_exitAction = fileMenu->addAction("退出(&X)");
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip("退出程序");

    QMenu *toolsMenu = menuBar->addMenu("工具(&T)");
    m_clearHistoryAction = toolsMenu->addAction("清除历史(&C)");
    m_clearHistoryAction->setStatusTip("清除所有历史记录");

    toolsMenu->addSeparator();

    m_settingsAction = toolsMenu->addAction("设置(&S)");
    m_settingsAction->setShortcut(QKeySequence::Preferences);
    m_settingsAction->setStatusTip("打开设置");

    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");
    m_aboutAction = helpMenu->addAction("关于(&A)");
    m_aboutAction->setShortcut(QKeySequence::HelpContents);
    m_aboutAction->setStatusTip("关于程序");
}

void MainWindow::createToolBar()
{
    QToolBar *toolBar = addToolBar("主工具栏");
    toolBar->setMovable(false);

    toolBar->addAction(m_newDetectionAction);
    toolBar->addAction(m_resumeDetectionAction);
    toolBar->addSeparator();
    toolBar->addAction(m_exportResultsAction);
    toolBar->addSeparator();
    toolBar->addAction(m_settingsAction);
}

void MainWindow::createStatusBar()
{
    QStatusBar *statusBar = this->statusBar();

    m_statusLabel = new QLabel("就绪", this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumWidth(300);

    statusBar->addPermanentWidget(m_statusLabel, 1);
    statusBar->addPermanentWidget(m_progressBar);
}

void MainWindow::connectSignals()
{
    connect(m_newDetectionAction, &QAction::triggered, this, &MainWindow::onNewDetection);
    connect(m_resumeDetectionAction, &QAction::triggered, this, &MainWindow::onResumeDetection);
    connect(m_exportResultsAction, &QAction::triggered, this, &MainWindow::onExportResults);
    connect(m_clearHistoryAction, &QAction::triggered, this, &MainWindow::onClearHistory);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onOpenSettings);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    connect(m_inputWidget, &ApiInputWidget::detectionStarted, this, &MainWindow::onDetectionStarted);
    connect(m_inputWidget, &ApiInputWidget::detectionProgress, this, &MainWindow::onDetectionProgress);
    connect(m_inputWidget, &ApiInputWidget::detectionFinished, this, &MainWindow::onDetectionFinished);
    connect(m_inputWidget, &ApiInputWidget::detectionError, this, &MainWindow::onDetectionError);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::onNewDetection()
{
    m_tabWidget->setCurrentWidget(m_inputWidget);
    m_inputWidget->clearInput();
}

void MainWindow::onResumeDetection()
{
    m_tabWidget->setCurrentWidget(m_historyWidget);
    m_historyWidget->showResumeDialog();
}

void MainWindow::onViewHistory()
{
    m_tabWidget->setCurrentWidget(m_historyWidget);
}

void MainWindow::onOpenSettings()
{
    m_tabWidget->setCurrentWidget(m_settingsWidget);
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "关于 API检测器",
        "<h2>API检测器 v2.0.0</h2>"
        "<p>高性能API有效性检测工具</p>"
        "<p><b>特性：</b></p>"
        "<ul>"
        "<li>✅ 支持自定义API端点</li>"
        "<li>⚡ 多线程并发检测</li>"
        "<li>📊 实时结果展示</li>"
        "<li>💾 历史记录管理</li>"
        "<li>🎨 现代化GUI界面</li>"
        "</ul>"
        "<p>基于C++和Qt6构建</p>"
        "<p>© 2024 API检测器团队</p>");
}

void MainWindow::onExportResults()
{
    m_resultWidget->exportResults();
}

void MainWindow::onClearHistory()
{
    auto reply = QMessageBox::question(this, "确认清除",
        "确定要清除所有历史记录吗？此操作不可恢复。",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_historyWidget->clearAllHistory();
    }
}

void MainWindow::onDetectionStarted(int total)
{
    m_statusLabel->setText(QString("正在检测 %1 个API...").arg(total));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(0);
    m_tabWidget->setCurrentWidget(m_resultWidget);
}

void MainWindow::onDetectionProgress(int current, int valid, int invalid, int error)
{
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("进度: %1 | 有效: %2 | 无效: %3 | 错误: %4")
        .arg(current).arg(valid).arg(invalid).arg(error));
}

void MainWindow::onDetectionFinished()
{
    m_progressBar->setVisible(false);
    m_statusLabel->setText("检测完成");
    m_resultWidget->refreshResults();
}

void MainWindow::onDetectionError(const QString &error)
{
    m_progressBar->setVisible(false);
    m_statusLabel->setText("检测失败");
    QMessageBox::critical(this, "检测错误", error);
}
