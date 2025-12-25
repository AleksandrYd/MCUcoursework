#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <qcombobox.h>
#include <qtableview.h>
#include <qtextedit.h>
#include "serialport.h"
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QTableView* table_view;
    QMap<quint64, int> addressToRow;

    QString port_name;
    SerialPort* port;

    QComboBox* bit_depth_cb;
    QComboBox* n_com_cb;
    quint64 selected_address = 0;

    QTextEdit* te_connection_state;
    QString connection_text;

    QStandardItemModel* model;

public slots:
    void onReceivedData(quint64 address, quint8 status, float temperature, quint8 bitdepth);
    void onConnectionState(QPair<QString, int>* connection_state);

private slots:
    void on_pushButton_3_clicked();
    void on_pushButton_2_clicked();
    void onTableSelectionChanged(const QItemSelection &selected, const QItemSelection &);

    void on_pushButton_clicked();

signals:
    void sendCommand(quint8 bit_depth, quint8 value, quint64 address);
};
#endif // MAINWINDOW_H
