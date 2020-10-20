#include "mainwindow.h"

#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(APP_NAME);
    QApplication::setApplicationVersion("1.0");

    QTranslator translator;
    translator.load(QLocale(), APP_SIMPLE_NAME, "_", ":/i18n");
    app.installTranslator(&translator);

    MainWindow w;
    w.show();

    return app.exec();
}
