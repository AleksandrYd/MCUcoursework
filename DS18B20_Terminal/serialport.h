#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QSerialPortInfo>
#include <QPair>

class SerialPort : public QObject
{
    Q_OBJECT
public:
    explicit SerialPort(QObject *parent = nullptr);
    void tryToConnect(QString com_name);
    void tryToDisconnect();
    QPair<QString, int> connection_state;

private:
    void proccessData(const QByteArray &data);

    QSerialPort* m_port;
    QString m_port_name;
    bool m_port_state;
    QByteArray rxBuffer;


    const QByteArray HEADER = QByteArray::fromHex("AA55");
    const int PACKET_SIZE = 16;

signals:
    void ConnectionState(QPair<QString, int>* connection_state);
    void ReceivedData(quint64 address, quint8 status, float temperature, quint8 bitdepth);

public slots:
    void onSendCommand(quint8 bit_depth, quint8 value, quint64 address);
private slots:
    void onReadyRead();
    void onDisconnect(QSerialPort::SerialPortError error);


};

#endif // SERIALPORT_H
