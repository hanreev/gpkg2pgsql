#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName("GeoPackage to PostgreSQL GUI");
    QApplication::setApplicationVersion("1.0");
    MainWindow w;
    w.show();
    return a.exec();
}
