#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>
#include <QFile>
#include <QTextStream>
#include "main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("API检测器");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("APIChecker");

    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));

    QFile styleFile(":/styles/dark_theme.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream ts(&styleFile);
        app.setStyleSheet(ts.readAll());
        styleFile.close();
    }

    MainWindow window;
    window.show();

    return app.exec();
}
