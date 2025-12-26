#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QLabel>
#include <QProgressBar>
#include <memory>
#include "api_input_widget.h"
#include "result_widget.h"
#include "history_widget.h"
#include "settings_widget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNewDetection();
    void onResumeDetection();
    void onViewHistory();
    void onOpenSettings();
    void onAbout();
    void onExportResults();
    void onClearHistory();
    void onDetectionStarted(int total);
    void onDetectionProgress(int current, int valid, int invalid, int error);
    void onDetectionFinished();
    void onDetectionError(const QString &error);

private:
    void setupUi();
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void connectSignals();
    void loadSettings();
    void saveSettings();

    QTabWidget *m_tabWidget;
    ApiInputWidget *m_inputWidget;
    ResultWidget *m_resultWidget;
    HistoryWidget *m_historyWidget;
    SettingsWidget *m_settingsWidget;

    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;

    QAction *m_newDetectionAction;
    QAction *m_resumeDetectionAction;
    QAction *m_exportResultsAction;
    QAction *m_clearHistoryAction;
    QAction *m_settingsAction;
    QAction *m_exitAction;
    QAction *m_aboutAction;
};
