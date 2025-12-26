#ifndef LAUNCHER_WINDOW_H
#define LAUNCHER_WINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>

class LauncherWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LauncherWindow(QWidget *parent = nullptr);
    ~LauncherWindow();

    void checkAndLaunch();

private slots:
    void onBuildOutput();
    void onBuildFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onBuildError(QProcess::ProcessError error);
    void onCloseClicked();

private:
    bool isGuiBuilt();
    bool areDependenciesInstalled();
    void startGui();
    void startBuild();
    void installDependencies();
    void appendLog(const QString &message, const QString &color = "black");

    QWidget *m_centralWidget;
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    QTextEdit *m_logTextEdit;
    QPushButton *m_closeButton;
    QProcess *m_buildProcess;
    bool m_isBuilding;
};

#endif
