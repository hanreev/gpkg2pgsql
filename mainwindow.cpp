#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QProcess>
#include <QSqlError>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    settings = new QSettings(QDir(qApp->applicationDirPath()).filePath(APP_SIMPLE_NAME + ".ini"), QSettings::IniFormat);
    db = QSqlDatabase::addDatabase("QPSQL");

    initUi();
    setupSignals();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUi()
{
    ui->setupUi(this);
    setWindowTitle(APP_NAME);

    QRect geometry = settings->value("UI/Geometry").toRect();
    if (!geometry.isEmpty()) setGeometry(geometry);

    ui->le_port->setValidator(new QIntValidator());

    ui->le_database->setText(settings->value("DB/Database").toString());
    ui->le_host->setText(settings->value("DB/Host").toString());
    ui->le_port->setText(settings->value("DB/Port").toString());
    ui->le_user->setText(settings->value("DB/User").toString());

    ui->tw_files->setContextMenuPolicy(Qt::CustomContextMenu);

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setupSignals()
{
    connect(ui->action_Open, &QAction::triggered, this, &MainWindow::addFiles);
    connect(ui->action_About, &QAction::triggered, this, &MainWindow::showAbout);

    connect(ui->btn_connect, &QPushButton::clicked, this, &MainWindow::connectDb);
    connect(ui->tw_files,&QWidget::customContextMenuRequested, this, &MainWindow::showFileMenu);
    connect(ui->btn_add_files, &QPushButton::clicked, this, &MainWindow::addFiles);
    connect(ui->btn_clear_files, &QPushButton::clicked, this, &MainWindow::clearFiles);
    connect(ui->btn_toggle_log, &QPushButton::clicked, this, &MainWindow::toggleLog);
    connect(ui->btn_process, &QPushButton::clicked, this, &MainWindow::process);
    connect(ui->btn_save_log, &QPushButton::clicked, this, &MainWindow::saveLog);
}

void MainWindow::alert(const QString &title, const QString &text, QMessageBox::Icon icon, bool writeToLog)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setIcon(icon);
    msgBox.setText(text);
    msgBox.exec();

    if (!writeToLog) return;

    LogLevel level;
    switch (icon) {
    case QMessageBox::Warning:
        level=WARNING;
        break;
    case QMessageBox::Critical:
        level=ERROR;
        break;
    default:
        level=INFO;
        break;
    }
    writeLog(text, level);
}

void MainWindow::writeLog(const QString &message, LogLevel level)
{
    QString lvl;
    switch (level) {
    case DEBUG:
        lvl = QString("DEBUG");
        break;
    case WARNING:
        lvl = QString("WARNING");
        break;
    case ERROR:
        lvl = QString("ERROR");
        break;
    default:
        lvl = QString("INFO");
        break;
    }
    writeRawLog(QString("[%1]: %2").arg(lvl).arg(message));
}

void MainWindow::writeRawLog(const QString &message)
{
    ui->te_log->append(message);
    ui->te_log->ensureCursorVisible();
    qApp->processEvents();
}

void MainWindow::addFiles()
{
    QString lastDir = settings->value("LastDir", QDir::homePath()).toString();
    QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Add GPKG file(s)"), lastDir, "GPKG (*.gpkg)");

    int rowCount = ui->tw_files->rowCount();

    QStringList existingFiles;
    for (int row = 0; row < rowCount; ++row) {
        existingFiles << ui->tw_files->item(row, 0)->text();
    }

    fileNames = (fileNames.toSet() - existingFiles.toSet()).toList();

    ui->tw_files->setRowCount(rowCount + fileNames.count());

    for (int i = 0; i < fileNames.count(); ++i) {
        QString fileName = fileNames.at(i);
        int row = rowCount + i;

        if (i == 0) {
            QDir fileDir(fileName);
            fileDir.cdUp();
            settings->setValue("LastDir", fileDir.path());
        }

        QTableWidgetItem *itemFileName = new QTableWidgetItem(fileName);
        itemFileName->setFlags(itemFileName->flags() ^ Qt::ItemIsEditable);
        ui->tw_files->setItem(row, 0, itemFileName);

        QFileInfo fileInfo(fileName);
        QStringList splits = fileInfo.fileName().split(".");
        splits.removeLast();
        QString tableName = splits.join(".").replace(QRegExp("\\W"), "_").toLower();
        ui->tw_files->setItem(row, 1, new QTableWidgetItem(tableName));

        QLineEdit *le_srid = new QLineEdit;
        le_srid->setFrame(false);
        le_srid->setValidator(new QIntValidator);
        ui->tw_files->setCellWidget(row, 2, le_srid);

        QWidget *w_ow = new QWidget;
        QCheckBox *cbx_ow = new QCheckBox;
        cbx_ow->setChecked(true);
        QHBoxLayout *lyt_ow = new QHBoxLayout(w_ow);
        lyt_ow->addWidget(cbx_ow);
        lyt_ow->setAlignment(Qt::AlignCenter);
        lyt_ow->setContentsMargins(0,0,0,0);
        ui->tw_files->setCellWidget(row, 3, w_ow);
    }

    ui->tw_files->resizeColumnsToContents();
}

void MainWindow::clearFiles()
{
    if (ui->tw_files->rowCount() < 1) return;

    if (QMessageBox::question(this, tr("Clear Files"), tr("Are you sure want to clear files?")) == QMessageBox::Yes) {
        ui->tw_files->clearContents();
        ui->tw_files->setRowCount(0);
        ui->tw_files->resizeColumnsToContents();
    }
}

void MainWindow::connectDb()
{
    QList<QLineEdit *> lineEdits = ui->gb_db_connection->findChildren<QLineEdit *>();

    if (db.isOpen()) {
        db.close();
        ui->btn_connect->setText(tr("Connect"));
        foreach (QLineEdit *lineEdit, lineEdits) {
            lineEdit->setEnabled(true);
        }
        writeLog(tr("Databse disconnected."));
        return;
    }

    QString dbName = ui->le_database->text();
    if (dbName.isEmpty()) {
        alert(tr("Error"), tr("Database name is required"), QMessageBox::Critical);
        ui->le_database->setFocus();
        return;
    }
    db.setDatabaseName(dbName);
    settings->setValue("DB/Database", dbName);

    QString host = ui->le_host->text();
    db.setHostName(host);
    settings->setValue("DB/Host", host);

    int port = ui->le_port->text().toInt();
    if (port > 0) {
        db.setPort(port);
        settings->setValue("DB/Port", port);
    }

    QString user = ui->le_user->text();
    db.setUserName(user);
    settings->setValue("DB/User", user);

    db.setPassword(ui->le_password->text());

    db.open();

    if (db.isOpen()) {
        writeLog(tr("Database connected."));
        foreach (QLineEdit *lineEdit, lineEdits) {
            lineEdit->setDisabled(true);
        }
        ui->btn_connect->setText(tr("Disconnect"));
    } else {
        writeLog(db.lastError().text(), ERROR);
        alert(tr("Error"), tr("Could not connect to database."), QMessageBox::Critical);
    }
}

void MainWindow::process()
{
    if (!db.isOpen()) {
        alert(tr("Error"), tr("Database not connected."), QMessageBox::Critical);
        return;
    }


    int rowCount = ui->tw_files->rowCount();
    if (rowCount < 1) {
        alert(tr("Error"), tr("No file available."), QMessageBox::Critical);
        return;
    }

    ui->btn_connect->setDisabled(true);
    ui->gb_files->setDisabled(true);
    qApp->processEvents();

    QStringList errors;
    QList<int> successRows;
    QProcess *proc = new QProcess(this);

    for (int row = 0; row < rowCount; ++row) {
        QString fileName(ui->tw_files->item(row, 0)->text());
        QString tableName(ui->tw_files->item(row, 1)->text());
        QLineEdit *le_srid = reinterpret_cast<QLineEdit *>(ui->tw_files->cellWidget(row, 2));
        QString srid(le_srid->text());
        QWidget *w_ow = ui->tw_files->cellWidget(row, 3);
        QCheckBox *cbx_ow = w_ow->findChild<QCheckBox *>();

        QString pgConnection = QString("PG:dbname=%1 host=%2 port=%3 user=%4 password=%5")
                .arg(db.databaseName())
                .arg(db.hostName().isEmpty() ? "127.0.0.1" : db.hostName())
                .arg(db.port() > 0 ? db.port() : 5432)
                .arg(db.userName())
                .arg(db.password());
        QStringList args;
        args << "-v" << "-f" << "PostgreSQL" << pgConnection;
        if (cbx_ow->isChecked()) {
            args << "-overwrite" << "-lco" << "OVERWRITE=YES";
        }
        if (!tableName.isEmpty()) {
            args << "-nln" << tableName;
        }
        if (!srid.isEmpty()) {
            args << "-t_srs" << "EPSG:" + srid;
        }

        args << fileName;

        writeRawLog("\n==========");
        writeLog(QString(tr("Processing %1.")).arg(fileName));

        try {
            proc->start("ogr2ogr", args);

            if (proc->waitForFinished() && proc->exitCode() == 0) {
                successRows.append(row);
                writeLog(QString(tr("Done processing %1.")).arg(fileName));
            } else {
                errors << fileName;
                writeLog(QString(tr("Error processing %1.")).arg(fileName), ERROR);
            }

            QString output(proc->readAll());
            if (!output.isEmpty())
                writeRawLog(QString("[OUTPUT]: %1").arg(output));
        } catch (...) {
            errors << fileName;
            writeLog(QString(tr("Error processing %1.")).arg(fileName), ERROR);
        }
        writeRawLog("==========");
    }

    ui->btn_connect->setEnabled(true);
    ui->gb_files->setEnabled(true);

    for (int i = successRows.count(); i > 0; --i) {
        ui->tw_files->removeRow(successRows.at(i - 1));
    }
    ui->tw_files->resizeColumnsToContents();

    if (errors.count() > 0) {
        alert(tr("Warning"), tr("There are some errors when processing files.\nCheck log for details."), QMessageBox::Warning, false);
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About"), QString(tr("<h3>%1</h3><p>Import GPKG files into PostgreSQL database.</p>")).arg(APP_NAME));
}

void MainWindow::showFileMenu(const QPoint &pos)
{
    QTableWidgetItem *item = ui->tw_files->itemAt(pos);
    if (!item) return;
    ui->tw_files->selectRow(item->row());
    QMenu menu(ui->tw_files);
    QAction *removeAction = menu.addAction(tr("Remove"));
    QAction *action = menu.exec(ui->tw_files->mapToGlobal(pos));
    if (action == removeAction) {
        ui->tw_files->removeRow(item->row());
        ui->tw_files->resizeColumnsToContents();
    }
}

void MainWindow::toggleLog()
{
    if (ui->gb_log->isVisible()) {
        ui->gb_log->hide();
        ui->btn_toggle_log->setText(tr("Show Log"));
    } else {
        ui->gb_log->show();
        ui->btn_toggle_log->setText(tr("Hide Log"));
    }
}

void MainWindow::saveLog()
{
    QString log = ui->te_log->toPlainText();
    if (log.isEmpty())
        return alert(tr("Error"), tr("Log is empty"), QMessageBox::Critical, false);

    QString lastSaveDir = settings->value("LastSaveDir", QDir::homePath()).toString();
    QString logFilePath = QFileDialog::getSaveFileName(this, tr("Save As"), lastSaveDir, QString("%1 (*.log *.txt);;%2 (*.*)").arg(tr("Text files")).arg(tr("All Files")));

    if (logFilePath.isEmpty()) return;

    QDir logDir(logFilePath);
    logDir.cdUp();
    settings->setValue("LastSaveDir", logDir.path());

    QFile logFile(logFilePath);
    if (logFile.open(QIODevice::WriteOnly)) {
        logFile.write(log.toUtf8());
        logFile.close();
        alert(tr("Success"), QString(tr("Logs successfully saved to %1.")).arg(logFilePath), QMessageBox::Information, false);
    } else {
        alert(tr("Error"), QString(tr("%1 is not writeable.")).arg(logFilePath), QMessageBox::Critical, false);
    }
}

void MainWindow::writeSettings()
{
    settings->setValue("UI/Geometry", geometry());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (QMessageBox::question(this, tr("Exit"), tr("Are you sure want to exit?")) == QMessageBox::Yes) {
        writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}
