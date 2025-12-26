#include "api_input_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSplitter>

ApiInputWidget::ApiInputWidget(QWidget *parent)
    : QWidget(parent), m_isRunning(false)
{
    setupUi();
    connectSignals();
}

void ApiInputWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    m_inputGroup = new QGroupBox("API输入", this);
    QVBoxLayout *inputLayout = new QVBoxLayout(m_inputGroup);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_loadFileButton = new QPushButton("📁 从文件加载", this);
    m_clearButton = new QPushButton("🗑️ 清空", this);
    buttonLayout->addWidget(m_loadFileButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_clearButton);

    m_apiInput = new QTextEdit(this);
    m_apiInput->setPlaceholderText("在此输入API，每行一个...\n\n示例：\nsk-xxxxxxxxxxxxxxxxxxxxxxxx\nsk-yyyyyyyyyyyyyyyyyyyyyyyy");
    m_apiInput->setMinimumHeight(200);

    m_validationLabel = new QLabel("✓ 输入有效", this);
    m_validationLabel->setStyleSheet("color: green;");

    inputLayout->addLayout(buttonLayout);
    inputLayout->addWidget(m_apiInput);
    inputLayout->addWidget(m_validationLabel);

    m_configGroup = new QGroupBox("检测配置", this);
    QFormLayout *configLayout = new QFormLayout(m_configGroup);

    m_endpointInput = new QLineEdit("https://api.openai.com/v1/models", this);
    m_endpointInput->setPlaceholderText("https://api.example.com/endpoint");

    m_methodCombo = new QComboBox(this);
    m_methodCombo->addItems({"GET", "POST", "PUT", "DELETE", "PATCH"});
    m_methodCombo->setCurrentText("GET");

    m_headersInput = new QLineEdit("Content-Type: application/json", this);
    m_headersInput->setPlaceholderText("Header1: Value1; Header2: Value2");

    m_requestBodyInput = new QTextEdit(this);
    m_requestBodyInput->setPlaceholderText("请求体内容（JSON格式）");
    m_requestBodyInput->setMaximumHeight(80);

    m_concurrentSpin = new QSpinBox(this);
    m_concurrentSpin->setRange(1, 5000);
    m_concurrentSpin->setValue(1000);
    m_concurrentSpin->setSuffix(" 个");

    m_timeoutSpin = new QSpinBox(this);
    m_timeoutSpin->setRange(1, 120);
    m_timeoutSpin->setValue(10);
    m_timeoutSpin->setSuffix(" 秒");

    m_saveProgressCheck = new QCheckBox("自动保存进度", this);
    m_saveProgressCheck->setChecked(true);

    configLayout->addRow("API端点:", m_endpointInput);
    configLayout->addRow("HTTP方法:", m_methodCombo);
    configLayout->addRow("请求头:", m_headersInput);
    configLayout->addRow("请求体:", m_requestBodyInput);
    configLayout->addRow("并发数:", m_concurrentSpin);
    configLayout->addRow("超时时间:", m_timeoutSpin);
    configLayout->addRow("", m_saveProgressCheck);

    m_advancedGroup = new QGroupBox("操作", this);
    QHBoxLayout *actionLayout = new QHBoxLayout(m_advancedGroup);

    m_startButton = new QPushButton("🚀 开始检测", this);
    m_startButton->setMinimumHeight(40);
    m_startButton->setStyleSheet("font-weight: bold; font-size: 14px;");

    m_stopButton = new QPushButton("⏹️ 停止", this);
    m_stopButton->setMinimumHeight(40);
    m_stopButton->setEnabled(false);

    m_statusLabel = new QLabel("就绪", this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);

    actionLayout->addWidget(m_startButton);
    actionLayout->addWidget(m_stopButton);
    actionLayout->addStretch();
    actionLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(m_inputGroup);
    mainLayout->addWidget(m_configGroup);
    mainLayout->addWidget(m_advancedGroup);
    mainLayout->addWidget(m_progressBar);
}

void ApiInputWidget::connectSignals()
{
    connect(m_loadFileButton, &QPushButton::clicked, this, &ApiInputWidget::onLoadFromFile);
    connect(m_clearButton, &QPushButton::clicked, this, &ApiInputWidget::onClearInput);
    connect(m_startButton, &QPushButton::clicked, this, &ApiInputWidget::onStartDetection);
    connect(m_stopButton, &QPushButton::clicked, this, &ApiInputWidget::onStopDetection);
    connect(m_apiInput, &QTextEdit::textChanged, this, &ApiInputWidget::onValidationChanged);
}

void ApiInputWidget::clearInput()
{
    m_apiInput->clear();
    m_endpointInput->setText("https://api.openai.com/v1/models");
    m_methodCombo->setCurrentText("GET");
    m_headersInput->setText("Content-Type: application/json");
    m_requestBodyInput->clear();
    m_concurrentSpin->setValue(1000);
    m_timeoutSpin->setValue(10);
    m_progressBar->setVisible(false);
    m_statusLabel->setText("就绪");
}

QStringList ApiInputWidget::getApiKeys() const
{
    QStringList keys;
    QString text = m_apiInput->toPlainText();
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    QRegularExpression regex("^\\s*sk-");
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && regex.match(trimmed).hasMatch()) {
            keys.append(trimmed);
        }
    }

    return keys;
}

QString ApiInputWidget::getApiEndpoint() const
{
    return m_endpointInput->text().trimmed();
}

QString ApiInputWidget::getHttpMethod() const
{
    return m_methodCombo->currentText();
}

int ApiInputWidget::getConcurrent() const
{
    return m_concurrentSpin->value();
}

int ApiInputWidget::getTimeout() const
{
    return m_timeoutSpin->value();
}

QString ApiInputWidget::getHeaders() const
{
    return m_headersInput->text().trimmed();
}

QString ApiInputWidget::getRequestBody() const
{
    return m_requestBodyInput->toPlainText().trimmed();
}

void ApiInputWidget::onLoadFromFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择API文件",
        "", "文本文件 (*.txt);;所有文件 (*.*)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            m_apiInput->setPlainText(in.readAll());
            file.close();
            m_statusLabel->setText(QString("已加载: %1").arg(QFileInfo(fileName).fileName()));
        }
    }
}

void ApiInputWidget::onClearInput()
{
    m_apiInput->clear();
    m_statusLabel->setText("已清空");
}

void ApiInputWidget::onValidationChanged()
{
    updateValidationStatus();
}

void ApiInputWidget::updateValidationStatus()
{
    QStringList keys = getApiKeys();

    if (keys.isEmpty()) {
        m_validationLabel->setText("⚠️ 请输入API（以sk-开头）");
        m_validationLabel->setStyleSheet("color: orange;");
        m_startButton->setEnabled(false);
    } else {
        m_validationLabel->setText(QString("✓ 检测到 %1 个有效API").arg(keys.size()));
        m_validationLabel->setStyleSheet("color: green;");
        m_startButton->setEnabled(!m_isRunning);
    }
}

bool ApiInputWidget::validateInput()
{
    if (m_apiInput->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入至少一个API");
        return false;
    }

    QStringList keys = getApiKeys();
    if (keys.isEmpty()) {
        QMessageBox::warning(this, "格式错误", "未找到有效的API（必须以sk-开头）");
        return false;
    }

    if (m_endpointInput->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "配置错误", "请输入API端点URL");
        return false;
    }

    return true;
}

void ApiInputWidget::onStartDetection()
{
    if (!validateInput()) {
        return;
    }

    QStringList keys = getApiKeys();
    m_isRunning = true;
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_loadFileButton->setEnabled(false);
    m_clearButton->setEnabled(false);

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, keys.size());
    m_progressBar->setValue(0);

    m_statusLabel->setText("正在初始化...");

    emit detectionStarted(keys.size());

    m_checkerThread = new CheckerThread(this);
    m_checkerThread->setApiKeys(keys);
    m_checkerThread->setEndpoint(getApiEndpoint());
    m_checkerThread->setMethod(getHttpMethod());
    m_checkerThread->setHeaders(getHeaders());
    m_checkerThread->setRequestBody(getRequestBody());
    m_checkerThread->setConcurrent(getConcurrent());
    m_checkerThread->setTimeout(getTimeout());

    connect(m_checkerThread, &CheckerThread::progress, this, &ApiInputWidget::onCheckerProgress);
    connect(m_checkerThread, &CheckerThread::finished, this, &ApiInputWidget::onCheckerFinished);
    connect(m_checkerThread, &CheckerThread::error, this, &ApiInputWidget::onCheckerError);

    m_checkerThread->start();
}

void ApiInputWidget::onStopDetection()
{
    if (m_checkerThread && m_checkerThread->isRunning()) {
        m_checkerThread->stop();
        m_statusLabel->setText("正在停止...");
    }
}

void ApiInputWidget::onCheckerProgress(int current, int valid, int invalid, int error)
{
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("进度: %1/%2 | 有效: %3 | 无效: %4 | 错误: %5")
        .arg(current).arg(m_progressBar->maximum()).arg(valid).arg(invalid).arg(error));

    emit detectionProgress(current, valid, invalid, error);
}

void ApiInputWidget::onCheckerFinished()
{
    m_isRunning = false;
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_loadFileButton->setEnabled(true);
    m_clearButton->setEnabled(true);

    m_progressBar->setValue(m_progressBar->maximum());
    m_statusLabel->setText("检测完成");

    m_checkerThread->deleteLater();
    m_checkerThread = nullptr;

    emit detectionFinished();
}

void ApiInputWidget::onCheckerError(const QString &error)
{
    m_isRunning = false;
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_loadFileButton->setEnabled(true);
    m_clearButton->setEnabled(true);

    m_progressBar->setVisible(false);
    m_statusLabel->setText("检测失败");

    m_checkerThread->deleteLater();
    m_checkerThread = nullptr;

    emit detectionError(error);
}
