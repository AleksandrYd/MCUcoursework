#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QtSerialPort/QSerialPort>
#include <QSerialPortInfo>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("Удаленный контроль");

    n_com_cb = ui->comboBox_2;
    bit_depth_cb = ui->comboBox;

    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        n_com_cb->addItem(info.portName());
    }

    QStringList lst;
    lst << "9" << "10" << "11" << "12";
    bit_depth_cb->addItems(lst);

    port = new SerialPort(this);
    connect(port, &SerialPort::ReceivedData, this, &MainWindow::onReceivedData, Qt::QueuedConnection);
    connect(port, &SerialPort::ConnectionState, this, &MainWindow::onConnectionState);
    connect(this, &MainWindow::sendCommand, port, &SerialPort::onSendCommand, Qt::QueuedConnection);

    table_view = ui->tableView;
    model =  new QStandardItemModel(this);

    model->setColumnCount(4);
    model->setHorizontalHeaderLabels({
        "Адрес",
        "Состояние",
        "Температура",
        "Разрядность"
    });

    table_view->setModel(model);

    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    te_connection_state = ui->textEdit;

    connect(table_view->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &MainWindow::onTableSelectionChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::onReceivedData(quint64 rec_address, quint8 rec_status, float rec_temperature, quint8 rec_bitdepth)
{
    int row;

    // Если устройство уже есть — берём его строку
    if (addressToRow.contains(rec_address))
    {
        row = addressToRow[rec_address];

        model->item(row,1)->setText(QString::number(rec_status));
        model->item(row,2)->setText(QString::number(rec_temperature,'f',4));
        model->item(row,3)->setText(QString::number(rec_bitdepth));

        QColor color = (rec_status == 0) ? Qt::green : Qt::red;
        for (int col = 0; col < model->columnCount(); ++col)
            model->item(row, col)->setBackground(color.lighter(180));
    }
    else
    {
        // Новое устройство
        row = model->rowCount();
        addressToRow[rec_address] = row;
        model->insertRow(row);
        model->setItem(row, 0,
                       new QStandardItem(QString("0x%1").arg(rec_address, 0, 16)));
        model->setItem(row, 1,
                       new QStandardItem(QString::number(rec_status)));
        model->setItem(row, 2,
                       new QStandardItem(QString::number(rec_temperature, 'f', 4 )));
        model->setItem(row, 3,
                       new QStandardItem(QString::number(rec_bitdepth)));
    }
}

void MainWindow::on_pushButton_3_clicked()
{
    port_name = n_com_cb->currentText();
    port->tryToConnect(port_name);
}

void MainWindow::onConnectionState(QPair<QString, int>* connection_state)
{
    connection_text = connection_state->first;
    switch (connection_state->second)
    {
    case 0:
        te_connection_state->setStyleSheet("background-color: green");
        break;
    case 1:
    case 2:
        te_connection_state->setStyleSheet("background-color: red");
        break;
    case 3:
        te_connection_state->setStyleSheet("background-color: yellow");
        break;
    default:
        te_connection_state->setStyleSheet("background-color: yellow");
        break;
    }

    te_connection_state->setText(connection_text);
}

void MainWindow::onTableSelectionChanged(const QItemSelection &selected, const QItemSelection &)
{
    if (selected.indexes().isEmpty())
        return;

    int row = selected.indexes().first().row();

    // Адрес лежит в 0 колонке
    QString addrStr = model->item(row, 0)->text();

    // "0x..." → quint64
    selected_address = addrStr.toULongLong(nullptr, 16);
}

void MainWindow::on_pushButton_2_clicked()
{
    if (selected_address == 0)
        return;
    quint8 command = 0x01;
    quint8 value = bit_depth_cb->currentText().toInt();
    emit sendCommand(command, value, selected_address);
}


void MainWindow::on_pushButton_clicked()
{
    port->tryToDisconnect();
}

