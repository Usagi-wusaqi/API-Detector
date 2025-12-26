#include "settings_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFormLayout>

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    connectSignals();
    loadSettings();
}

void SettingsWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    m_tabWidget = new QTabWidget(this);

    QWidget *detectionTab = new QWidget(this);
    QVBoxLayout *detectionLayout = new QVBoxLayout(detectionTab);

    QGroupBox *basicGroup = new QGroupBox("基本设置", detectionTab);
    QFormLayout *basicForm = new QFormLayout(basicGroup);

    m_defaultConcurrentSpin = new QSpinBox(detectionTab);
    m_defaultConcurrentSpin->setRange(1, 5000);
    m_defaultConcurrentSpin->setValue(1000);
    m_defaultConcurrentSpin->setSuffix(" 个");

    m_defaultTimeoutSpin = new QSpinBox(detectionTab);
    m_defaultTimeoutSpin->setRange(1, 120);
    m_defaultTimeoutSpin->setValue(10);
    m_defaultTimeoutSpin->setSuffix(" 秒");

    m_connectTimeoutSpin = new QSpinBox(detectionTab);
    m_connectTimeoutSpin->setRange(1, 60);
    m_connectTimeoutSpin->setValue(5);
    m_connectTimeoutSpin->setSuffix(" 秒");

    basicForm->addRow("默认并发数:", m_defaultConcurrentSpin);
    basicForm->addRow("默认超时时间:", m_defaultTimeoutSpin);
    basicForm->addRow("连接超时时间:", m_connectTimeoutSpin);

    QGroupBox *endpointGroup = new QGroupBox("API端点设置", detectionTab);
    QFormLayout *endpointForm = new QFormLayout(endpointGroup);

    m_defaultEndpointEdit = new QLineEdit("https://api.openai.com/v1/models", detectionTab);
    m_defaultEndpointEdit->setPlaceholderText("https://api.example.com/endpoint");

    m_defaultMethodCombo = new QComboBox(detectionTab);
    m_defaultMethodCombo->addItems({"GET", "POST", "PUT", "DELETE", "PATCH"});
    m_defaultMethodCombo->setCurrentText("GET");

    endpointForm->addRow("默认端点:", m_defaultEndpointEdit);
    endpointForm->addRow("默认方法:", m_defaultMethodCombo);

    QGroupBox *customGroup = new QGroupBox("自定义请求", detectionTab);
    QVBoxLayout *customLayout = new QVBoxLayout(customGroup);

    m_customHeadersEdit = new QLineEdit(detectionTab);
    m_customHeadersEdit->setPlaceholderText("Header1: Value1; Header2: Value2");

    m_customBodyEdit = new QTextEdit(detectionTab);
    m_customBodyEdit->setPlaceholderText("自定义请求体（JSON格式）");
    m_customBodyEdit->setMaximumHeight(80);

    customLayout->addWidget(new QLabel("自定义请求头:", detectionTab));
    customLayout->addWidget(m_customHeadersEdit);
    customLayout->addWidget(new QLabel("自定义请求体:", detectionTab));
    customLayout->addWidget(m_customBodyEdit);

    detectionLayout->addWidget(basicGroup);
    detectionLayout->addWidget(endpointGroup);
    detectionLayout->addWidget(customGroup);
    detectionLayout->addStretch();

    QWidget *progressTab = new QWidget(this);
    QVBoxLayout *progressLayout = new QVBoxLayout(progressTab);

    QGroupBox *saveGroup = new QGroupBox("进度保存", progressTab);
    QFormLayout *saveForm = new QFormLayout(saveGroup);

    m_autoSaveCheck = new QCheckBox("自动保存进度", progressTab);
    m_autoSaveCheck->setChecked(true);

    m_saveIntervalSpin = new QSpinBox(progressTab);
    m_saveIntervalSpin->setRange(5, 300);
    m_saveIntervalSpin->setValue(30);
    m_saveIntervalSpin->setSuffix(" 秒");

    m_autoResumeCheck = new QCheckBox("启动时自动恢复", progressTab);
    m_autoResumeCheck->setChecked(false);

    m_maxHistorySpin = new QSpinBox(progressTab);
    m_maxHistorySpin->setRange(0, 1000);
    m_maxHistorySpin->setValue(100);
    m_maxHistorySpin->setSpecialValueText("不限制");
    m_maxHistorySpin->setSuffix(" 条");

    saveForm->addRow(m_autoSaveCheck);
    saveForm->addRow("保存间隔:", m_saveIntervalSpin);
    saveForm->addRow(m_autoResumeCheck);
    saveForm->addRow("最大历史记录:", m_maxHistorySpin);

    progressLayout->addWidget(saveGroup);
    progressLayout->addStretch();

    QWidget *interfaceTab = new QWidget(this);
    QVBoxLayout *interfaceLayout = new QVBoxLayout(interfaceTab);

    QGroupBox *displayGroup = new QGroupBox("界面显示", interfaceTab);
    QFormLayout *displayForm = new QFormLayout(displayGroup);

    m_showProgressBarCheck = new QCheckBox("显示进度条", interfaceTab);
    m_showProgressBarCheck->setChecked(true);

    m_coloredOutputCheck = new QCheckBox("彩色输出", interfaceTab);
    m_coloredOutputCheck->setChecked(true);

    m_logLevelCombo = new QComboBox(interfaceTab);
    m_logLevelCombo->addItems({"调试", "信息", "警告", "错误"});
    m_logLevelCombo->setCurrentText("信息");

    displayForm->addRow(m_showProgressBarCheck);
    displayForm->addRow(m_coloredOutputCheck);
    displayForm->addRow("日志级别:", m_logLevelCombo);

    interfaceLayout->addWidget(displayGroup);
    interfaceLayout->addStretch();

    m_tabWidget->addTab(detectionTab, "🔍 检测设置");
    m_tabWidget->addTab(progressTab, "💾 进度设置");
    m_tabWidget->addTab(interfaceTab, "🎨 界面设置");

    QGroupBox *actionGroup = new QGroupBox("操作", this);
    QHBoxLayout *actionLayout = new QHBoxLayout(actionGroup);

    m_saveButton = new QPushButton("💾 保存设置", this);
    m_saveButton->setMinimumWidth(120);

    m_resetButton = new QPushButton("🔄 重置", this);
    m_resetButton->setMinimumWidth(100);

    m_restoreButton = new QPushButton("↩️ 恢复默认", this);
    m_restoreButton->setMinimumWidth(120);

    m_exportButton = new QPushButton("📤 导出配置", this);
    m_exportButton->setMinimumWidth(120);

    m_importButton = new QPushButton("📥 导入配置", this);
    m_importButton->setMinimumWidth(120);

    actionLayout->addWidget(m_saveButton);
    actionLayout->addWidget(m_resetButton);
    actionLayout->addWidget(m_restoreButton);
    actionLayout->addStretch();
    actionLayout->addWidget(m_exportButton);
    actionLayout->addWidget(m_importButton);

    mainLayout->addWidget(m_tabWidget);
    mainLayout->addWidget(actionGroup);
}

void SettingsWidget::connectSignals()
{
    connect(m_saveButton, &QPushButton::clicked, this, &SettingsWidget::onSave);
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsWidget::onReset);
    connect(m_restoreButton, &QPushButton::clicked, this, &SettingsWidget::onRestoreDefaults);
    connect(m_exportButton, &QPushButton::clicked, this, &SettingsWidget::onExportConfig);
    connect(m_importButton, &QPushButton::clicked, this, &SettingsWidget::onImportConfig);
}

void SettingsWidget::loadSettings()
{
    QSettings settings;

    m_defaultConcurrentSpin->setValue(settings.value("detection/default_concurrent", 1000).toInt());
    m_defaultTimeoutSpin->setValue(settings.value("detection/default_timeout", 10).toInt());
    m_connectTimeoutSpin->setValue(settings.value("detection/connect_timeout", 5).toInt());
    m_defaultEndpointEdit->setText(settings.value("detection/default_endpoint",
        "https://api.openai.com/v1/models").toString());
    m_defaultMethodCombo->setCurrentText(settings.value("detection/default_method", "GET").toString());

    m_autoSaveCheck->setChecked(settings.value("progress/auto_save", true).toBool());
    m_saveIntervalSpin->setValue(settings.value("progress/save_interval", 30).toInt());
    m_autoResumeCheck->setChecked(settings.value("progress/auto_resume", false).toBool());
    m_maxHistorySpin->setValue(settings.value("progress/max_history", 100).toInt());

    m_showProgressBarCheck->setChecked(settings.value("ui/show_progress", true).toBool());
    m_coloredOutputCheck->setChecked(settings.value("ui/colored_output", true).toBool());
    m_logLevelCombo->setCurrentText(settings.value("ui/log_level", "信息").toString());

    m_customHeadersEdit->setText(settings.value("detection/custom_headers", "").toString());
    m_customBodyEdit->setPlainText(settings.value("detection/custom_body", "").toString());
}

void SettingsWidget::saveSettings()
{
    QSettings settings;

    settings.setValue("detection/default_concurrent", m_defaultConcurrentSpin->value());
    settings.setValue("detection/default_timeout", m_defaultTimeoutSpin->value());
    settings.setValue("detection/connect_timeout", m_connectTimeoutSpin->value());
    settings.setValue("detection/default_endpoint", m_defaultEndpointEdit->text());
    settings.setValue("detection/default_method", m_defaultMethodCombo->currentText());

    settings.setValue("progress/auto_save", m_autoSaveCheck->isChecked());
    settings.setValue("progress/save_interval", m_saveIntervalSpin->value());
    settings.setValue("progress/auto_resume", m_autoResumeCheck->isChecked());
    settings.setValue("progress/max_history", m_maxHistorySpin->value());

    settings.setValue("ui/show_progress", m_showProgressBarCheck->isChecked());
    settings.setValue("ui/colored_output", m_coloredOutputCheck->isChecked());
    settings.setValue("ui/log_level", m_logLevelCombo->currentText());

    settings.setValue("detection/custom_headers", m_customHeadersEdit->text());
    settings.setValue("detection/custom_body", m_customBodyEdit->toPlainText());
}

void SettingsWidget::restoreDefaults()
{
    m_defaultConcurrentSpin->setValue(1000);
    m_defaultTimeoutSpin->setValue(10);
    m_connectTimeoutSpin->setValue(5);
    m_defaultEndpointEdit->setText("https://api.openai.com/v1/models");
    m_defaultMethodCombo->setCurrentText("GET");

    m_autoSaveCheck->setChecked(true);
    m_saveIntervalSpin->setValue(30);
    m_autoResumeCheck->setChecked(false);
    m_maxHistorySpin->setValue(100);

    m_showProgressBarCheck->setChecked(true);
    m_coloredOutputCheck->setChecked(true);
    m_logLevelCombo->setCurrentText("信息");

    m_customHeadersEdit->clear();
    m_customBodyEdit->clear();
}

void SettingsWidget::onSave()
{
    saveSettings();
    QMessageBox::information(this, "保存成功", "设置已保存");
}

void SettingsWidget::onReset()
{
    loadSettings();
    QMessageBox::information(this, "重置完成", "设置已重置为上次保存的值");
}

void SettingsWidget::onRestoreDefaults()
{
    auto reply = QMessageBox::question(this, "确认恢复",
        "确定要恢复所有设置为默认值吗？",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        restoreDefaults();
        QMessageBox::information(this, "恢复完成", "已恢复为默认设置");
    }
}

void SettingsWidget::onExportConfig()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出配置",
        "api_checker_config.json", "JSON文件 (*.json);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    QJsonObject config;
    config["default_concurrent"] = m_defaultConcurrentSpin->value();
    config["default_timeout"] = m_defaultTimeoutSpin->value();
    config["connect_timeout"] = m_connectTimeoutSpin->value();
    config["default_endpoint"] = m_defaultEndpointEdit->text();
    config["default_method"] = m_defaultMethodCombo->currentText();
    config["auto_save"] = m_autoSaveCheck->isChecked();
    config["save_interval"] = m_saveIntervalSpin->value();
    config["auto_resume"] = m_autoResumeCheck->isChecked();
    config["max_history"] = m_maxHistorySpin->value();
    config["show_progress"] = m_showProgressBarCheck->isChecked();
    config["colored_output"] = m_coloredOutputCheck->isChecked();
    config["log_level"] = m_logLevelCombo->currentText();
    config["custom_headers"] = m_customHeadersEdit->text();
    config["custom_body"] = m_customBodyEdit->toPlainText();

    QJsonDocument doc(config);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "导出失败", "无法打开文件进行写入");
        return;
    }

    file.write(doc.toJson());
    QMessageBox::information(this, "导出完成", "配置已导出");
}

void SettingsWidget::onImportConfig()
{
    QString fileName = QFileDialog::getOpenFileName(this, "导入配置",
        "", "JSON文件 (*.json);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "导入失败", "无法打开文件进行读取");
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isObject()) {
        QMessageBox::critical(this, "导入失败", "无效的配置文件格式");
        return;
    }

    QJsonObject config = doc.object();

    m_defaultConcurrentSpin->setValue(config.value("default_concurrent").toInt(1000));
    m_defaultTimeoutSpin->setValue(config.value("default_timeout").toInt(10));
    m_connectTimeoutSpin->setValue(config.value("connect_timeout").toInt(5));
    m_defaultEndpointEdit->setText(config.value("default_endpoint").toString("https://api.openai.com/v1/models"));
    m_defaultMethodCombo->setCurrentText(config.value("default_method").toString("GET"));
    m_autoSaveCheck->setChecked(config.value("auto_save").toBool(true));
    m_saveIntervalSpin->setValue(config.value("save_interval").toInt(30));
    m_autoResumeCheck->setChecked(config.value("auto_resume").toBool(false));
    m_maxHistorySpin->setValue(config.value("max_history").toInt(100));
    m_showProgressBarCheck->setChecked(config.value("show_progress").toBool(true));
    m_coloredOutputCheck->setChecked(config.value("colored_output").toBool(true));
    m_logLevelCombo->setCurrentText(config.value("log_level").toString("信息"));
    m_customHeadersEdit->setText(config.value("custom_headers").toString(""));
    m_customBodyEdit->setPlainText(config.value("custom_body").toString(""));

    QMessageBox::information(this, "导入完成", "配置已导入，请点击保存按钮保存");
}
