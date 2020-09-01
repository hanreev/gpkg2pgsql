# -*- coding: utf-8 -*-

import os
import re
import sys
from subprocess import call

from PyQt5.QtCore import QDir, QSettings, Qt
from PyQt5.QtGui import QIntValidator
from PyQt5.QtSql import QSqlDatabase
from PyQt5.QtWidgets import (QApplication, QCheckBox, QFileDialog, QHBoxLayout, QLineEdit, QMainWindow, QMenu,
                             QMessageBox, QTableWidgetItem, QWidget)

from ui_main import Ui_MainWindow

APP_NAME = 'GeoPackage to PostgreSQL GUI'
APP_SIMPLE_NAME = 'gpkg2pgsql'
BASE_DIR = os.path.abspath(os.path.dirname(__file__))
os.environ['PATH'] = os.environ['PATH'].rstrip(';') + ';' + os.path.join(BASE_DIR, 'runtime')
os.environ['GDAL_DATA'] = os.path.join(BASE_DIR, 'gdal')

settings = QSettings(os.path.join(BASE_DIR, APP_SIMPLE_NAME + '.ini'), QSettings.IniFormat)


class MainWindow(Ui_MainWindow, QMainWindow):
    def __init__(self):
        super(MainWindow, self).__init__()
        self.db = QSqlDatabase('QPSQL')
        self.is_db_connected = False
        self.init_ui()

    def init_ui(self):
        self.setupUi(self)
        self.setWindowTitle(APP_NAME)

        geometry = settings.value('UI/Geometry')
        if geometry:
            self.setGeometry(geometry)

        self.le_port.setValidator(QIntValidator())

        self.le_database.setText(settings.value('DB/Database'))
        self.le_host.setText(settings.value('DB/Host'))
        self.le_port.setText(settings.value('DB/Port'))
        self.le_user.setText(settings.value('DB/User'))

        self.action_Open.triggered.connect(self.add_files)
        self.action_About.triggered.connect(self.show_about)

        self.btn_connect.clicked.connect(self.connect_db)
        self.tw_files.setContextMenuPolicy(Qt.CustomContextMenu)
        self.tw_files.customContextMenuRequested.connect(self.show_file_menu)
        self.btn_add_files.clicked.connect(self.add_files)
        self.btn_clear_files.clicked.connect(self.clear_files)
        self.btn_toggle_log.clicked.connect(self.toggle_log)
        self.btn_process.clicked.connect(self.process)

        self.statusBar().showMessage('Ready')
        self.show()

    def alert(self, title, text, icon=QMessageBox.Information):
        mb = QMessageBox(icon, title, text, parent=self)
        level = 'info'
        if icon == QMessageBox.Warning:
            level = 'warning'
        elif icon == QMessageBox.Critical:
            level = 'error'
        self.write_log(text, level)
        mb.exec_()

    def write_log(self, message, level='info'):
        self.te_log.append('[{}]: {}'.format(level.upper(), str(message)))
        self.te_log.ensureCursorVisible()
        QApplication.processEvents()

    def connect_db(self):
        line_edits = self.gb_db_connection.findChildren((QLineEdit,))

        if self.db.isOpen():
            self.db.close()
            self.btn_connect.setText('Connect')
            for le in line_edits:
                le.setEnabled(True)
            self.write_log('Database disconnected.')
            return

        dbname = self.le_database.text()
        if not dbname:
            self.alert('Error', 'Database name is required', QMessageBox.Critical)
            self.le_database.setFocus()
            return

        self.db.setDatabaseName(dbname)
        settings.setValue('DB/Database', dbname)

        host = self.le_host.text()
        self.db.setHostName(host)
        settings.setValue('DB/Host', host)

        try:
            port = int(self.le_port.text())
            self.db.setPort(port)
            settings.setValue('DB/Port', port)
        except ValueError:
            pass

        user = self.le_user.text()
        self.db.setUserName(user)
        settings.setValue('DB/User', user)

        self.db.setPassword(self.le_password.text())

        self.db.open()
        if self.db.isOpen():
            self.write_log('Database connected.')
            for le in self.gb_db_connection.findChildren((QLineEdit,)):
                le.setDisabled(True)
            self.btn_connect.setText('Disconnect')
        else:
            self.write_log(self.db.lastError().text(), level='error')
            self.alert('Error', 'Could not connect to database.', QMessageBox.Critical)

    def add_files(self):
        last_dir = settings.value('LastDir', QDir.homePath())
        files, _ = QFileDialog.getOpenFileNames(self, 'Add GPKG file(s)', last_dir, filter='GPKG (*.gpkg)')
        if len(files):
            settings.setValue('LastDir', os.path.dirname(files[0]))
        row_count = self.tw_files.rowCount()
        self.tw_files.setRowCount(row_count + len(files))
        for i, filename in enumerate(files):
            row = row_count + i
            item_filename = QTableWidgetItem(filename)
            item_filename.setFlags(item_filename.flags() ^ Qt.ItemIsEditable)
            self.tw_files.setItem(row, 0, item_filename)
            basename, _ = os.path.splitext(os.path.basename(filename))
            self.tw_files.setItem(row, 1, QTableWidgetItem(re.sub(r'\W', '_', basename.lower())))
            le_srid = QLineEdit()
            le_srid.setFrame(False)
            le_srid.setValidator(QIntValidator())
            self.tw_files.setCellWidget(row, 2, le_srid)
            widget_ow = QWidget()
            cbx_ow = QCheckBox()
            cbx_ow.setChecked(True)
            lyt_ow = QHBoxLayout(widget_ow)
            lyt_ow.addWidget(cbx_ow)
            lyt_ow.setAlignment(Qt.AlignCenter)
            lyt_ow.setContentsMargins(0, 0, 0, 0)
            self.tw_files.setCellWidget(row, 3, widget_ow)
        self.tw_files.resizeColumnsToContents()

    def clear_files(self):
        result = QMessageBox.question(self, 'Clear Files', 'Are you sure want to clear files?')
        if result == QMessageBox.Yes:
            self.tw_files.clearContents()
            self.tw_files.setRowCount(0)
            self.tw_files.resizeColumnsToContents()

    def show_file_menu(self, pos):
        item = self.tw_files.itemAt(pos)
        if item is None:
            return
        self.tw_files.selectRow(item.row())
        menu = QMenu(self.tw_files)
        remove_action = menu.addAction('Remove')
        action = menu.exec_(self.tw_files.mapToGlobal(pos))
        if action == remove_action:
            self.tw_files.removeRow(item.row())
            self.tw_files.resizeColumnsToContents()

    def toggle_log(self):
        if self.gb_log.isVisible():
            self.gb_log.setVisible(False)
            self.btn_toggle_log.setText('Show Log')
        else:
            self.gb_log.setVisible(True)
            self.btn_toggle_log.setText('Hide Log')

    def process(self):
        if not self.db.isOpen():
            self.alert('Error', 'Database not connected.', QMessageBox.Critical)
            return

        row_count = self.tw_files.rowCount()
        if row_count < 1:
            self.alert('Error', 'No files available.', QMessageBox.Critical)
            return

        self.btn_connect.setDisabled(True)
        self.gb_files.setDisabled(True)

        QApplication.processEvents()

        errors = []
        success_rows = []

        for row in range(row_count):
            item_filename = self.tw_files.item(row, 0)
            item_table_name = self.tw_files.item(row, 1)
            le_srid = self.tw_files.cellWidget(row, 2)
            w_ow = self.tw_files.cellWidget(row, 3)
            cbx_ow = w_ow.findChild((QCheckBox,))
            dsn = 'dbname={} host={} port={} user={} password={}'.format(
                self.db.databaseName(),
                self.db.hostName() or '127.0.0.1',
                self.db.port() if self.db.port() > 0 else 5432,
                self.db.userName(),
                self.db.password()
            )
            table_name = item_table_name.text()
            srid = le_srid.text()
            args = ['ogr2ogr', '-f', 'PostgreSQL', 'PG:{}'.format(dsn)]
            if cbx_ow.isChecked():
                args += ['-overwrite', '-lco', 'OVERWRITE=YES']
            if table_name:
                args += ['-nln', table_name]
            if srid:
                args += ['-t_srs', 'EPSG:{}'.format(srid)]
            filename = item_filename.text()
            args.append(filename)
            self.write_log('Processing {}'.format(filename))
            try:
                retval = call(args, stdout=sys.stdout, stderr=sys.stderr)
                if retval == 0:
                    success_rows.append(row)
                else:
                    errors.append('Error processing {}'.format(filename))
            except Exception as e:
                errors.append(e)

        self.btn_connect.setEnabled(True)
        self.gb_files.setEnabled(True)

        for row in sorted(success_rows, reverse=True):
            self.tw_files.removeRow(row)
        self.tw_files.resizeColumnsToContents()

        if len(errors):
            for err in errors:
                self.write_log(err, 'error')
            QApplication.processEvents()

            self.alert(
                'Warning',
                'There are some errors when processing files.\nCheck log for details.',
                QMessageBox.Warning
            )

    def show_about(self):
        QMessageBox.about(
            self,
            'About',
            '<h3>{}</h3><p>Import GPKG files into PostgreSQL database.</p>'.format(APP_NAME)
        )

    def write_settings(self):
        settings.setValue('UI/Geometry', self.geometry())

    def closeEvent(self, event):
        result = QMessageBox.question(self, 'Exit', 'Are you sure want to exit?')
        if result == QMessageBox.Yes:
            self.write_settings()
            event.accept()
        else:
            event.ignore()


if __name__ == '__main__':
    app = QApplication(sys.argv)
    mw = MainWindow()
    sys.exit(app.exec_())
