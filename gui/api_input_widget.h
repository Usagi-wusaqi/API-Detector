#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include "checker_thread.h"

class ApiInputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ApiInputWidget(QWidget *parent = nullptr);

    void clearInput();
    QStringList getApiKeys() const;
    QString getApiEndpoint() const;
    QString getHttpMethod() const;
    int getConcurrent() const;
    int getTimeout() const;
    QString getHeaders() const;
    QString getRequestBody() const;

signals:
    void detectionStarted(int total);
    void detectionProgress(int current, int valid, int invalid, int error);
    void detectionFinished();
    void detectionError(const QString &error);

private slots:
    void onStartDetection();
    void onStopDetection();
    void onLoadFromFile();
    void onClearInput();
    void onValidationChanged();
    void onCheckerProgress(int current, int valid, int invalid, int error);
    void onCheckerFinished();
    void onCheckerError(const QString &error);

private:
    void setupUi();
    void connectSignals();
    bool validateInput();
    void updateValidationStatus();

    QGroupBox *m_inputGroup;
    QGroupBox *m_configGroup;
    QGroupBox *m_advancedGroup;

    QTextEdit *m_apiInput;
    QLabel *m_validationLabel;
    QPushButton *m_loadFileButton;
    QPushButton *m_clearButton;

    QLineEdit *m_endpointInput;
    QComboBox *m_methodCombo;
    QLineEdit *m_headersInput;
    QTextEdit *m_requestBodyInput;

    QSpinBox *m_concurrentSpin;
    QSpinBox *m_timeoutSpin;
    QCheckBox *m_saveProgressCheck;

    QPushButton *m_startButton;
    QPushButton *m_stopButton;

    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;

    CheckerThread *m_checkerThread;
    bool m_isRunning;
};
