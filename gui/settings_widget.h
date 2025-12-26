#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QSettings>

class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget *parent = nullptr);

private slots:
    void onSave();
    void onReset();
    void onRestoreDefaults();
    void onExportConfig();
    void onImportConfig();

private:
    void setupUi();
    void connectSignals();
    void loadSettings();
    void saveSettings();
    void restoreDefaults();

    QTabWidget *m_tabWidget;

    QSpinBox *m_defaultConcurrentSpin;
    QSpinBox *m_defaultTimeoutSpin;
    QSpinBox *m_connectTimeoutSpin;
    QLineEdit *m_defaultEndpointEdit;
    QComboBox *m_defaultMethodCombo;

    QCheckBox *m_autoSaveCheck;
    QSpinBox *m_saveIntervalSpin;
    QCheckBox *m_autoResumeCheck;
    QSpinBox *m_maxHistorySpin;

    QCheckBox *m_showProgressBarCheck;
    QCheckBox *m_coloredOutputCheck;
    QComboBox *m_logLevelCombo;

    QLineEdit *m_customHeadersEdit;
    QTextEdit *m_customBodyEdit;

    QPushButton *m_saveButton;
    QPushButton *m_resetButton;
    QPushButton *m_restoreButton;
    QPushButton *m_exportButton;
    QPushButton *m_importButton;
};
