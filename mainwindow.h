#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCloseEvent>
#include <QMainWindow>
#include <QMessageBox>
#include <QSettings>
#include <QSqlDatabase>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

const QString APP_NAME("GeoPackage to PostgreSQL GUI");
const QString APP_SIMPLE_NAME("gpkg2pgsql");

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };
    Q_ENUM(LogLevel)

public slots:
    void addFiles();
    void clearFiles();
    void connectDb();
    void process();
    void showAbout();
    void showFileMenu(const QPoint &pos);
    void toggleLog();
    void saveLog();

protected:
    void alert(const QString &title, const QString &text, QMessageBox::Icon icon = QMessageBox::Information, bool writeToLog = true);
    void closeEvent(QCloseEvent *event);
    void initUi();
    void setupSignals();
    void writeLog(const QString &message, LogLevel level = LogLevel::INFO);
    void writeRawLog(const QString &message);
    void writeSettings();

private:
    Ui::MainWindow *ui;
    QSettings *settings;
    QSqlDatabase db;
};
#endif // MAINWINDOW_H
