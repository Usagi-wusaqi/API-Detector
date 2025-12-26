#include <QApplication>
#include "launcher_window.h"
#include <QMessageBox>
#include <QDir>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("API检测器启动器");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("API检测器");
    
    LauncherWindow window;
    window.show();
    
    QTimer::singleShot(500, &window, &LauncherWindow::checkAndLaunch);
    
    return app.exec();
}
