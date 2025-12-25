#include "serialport.h"

SerialPort::SerialPort(QObject *parent)
    : QObject{parent}
{
    m_port = new QSerialPort(this);

    // Скорость передачи данных. 9600 бит/с.
    m_port->setBaudRate(QSerialPort::Baud9600);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_port, &QSerialPort::readyRead, this, &SerialPort::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialPort::onDisconnect);

    connection_state.first = "Подключение отсутстсвует";
    connection_state.second = 1;
}

void SerialPort::tryToConnect(QString com_name)
{
    if (!m_port->isOpen())
    {
        m_port_name = com_name;
        m_port->setPortName(m_port_name);

        if (m_port->open(QSerialPort::ReadWrite))
        {
            connection_state.first = "Подключение установлено";
            connection_state.second = 0;
            emit ConnectionState(&connection_state);
        }
        else
        {
            connection_state.first = m_port->errorString();
            connection_state.second = 2;
            emit ConnectionState(&connection_state);
        }
    }
}

void SerialPort::tryToDisconnect()
{
    if (m_port->isOpen())
    {
        m_port->close();
        connection_state.first = "Подключение отсутствует";
        connection_state.second = 1;
        emit ConnectionState(&connection_state);
    }
}

void SerialPort::onDisconnect(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::DeviceNotFoundError ||
        error == QSerialPort::NotOpenError ||
        error == QSerialPort::ResourceError ||
        error == QSerialPort::TimeoutError)
    {
        m_port->close();
        connection_state.first = "Подключение отсутствует или соединение разорвано";
        connection_state.second = 1;
        emit ConnectionState(&connection_state);
    }
}

void SerialPort::onReadyRead()
{
    rxBuffer.append(m_port->readAll());

    while (true)
    {
        //Ищем заголовок
        int headerIndex = rxBuffer.indexOf(HEADER);

        if (headerIndex < 0)
        {
            // Заголовок не найден — чистим мусор,
            // но оставляем последний байт (вдруг это начало HEADER)
            if (rxBuffer.size() > HEADER.size())
                rxBuffer.remove(0, rxBuffer.size() - HEADER.size());
            return;
        }

        //Удаляем всё перед заголовком
        if (headerIndex > 0)
            rxBuffer.remove(0, headerIndex);

        //Проверяем, есть ли весь пакет
        if (rxBuffer.size() < PACKET_SIZE)
            return; // ждём данные

        //Извлекаем пакет
        QByteArray packet = rxBuffer.mid(HEADER.size(),
                                         PACKET_SIZE - HEADER.size());

        //Удаляем пакет из буфера
        rxBuffer.remove(0, PACKET_SIZE);

        //Разбор
        proccessData(packet);
    }
}


void SerialPort::proccessData(const QByteArray &data)
{
    if (data.size() < 14) return;

    const char* ptr = data.constData();
    quint64 address;
    memcpy(&address, ptr, sizeof(address));
    ptr += 8;

    quint8 status = *reinterpret_cast<const quint8*>(ptr);
    ptr += 1;

    float temperature;
    memcpy(&temperature, ptr, sizeof(temperature));
    ptr += 4;

    quint8 bit_depth = *reinterpret_cast<const quint8*>(ptr);

    emit ReceivedData(address, status, temperature, bit_depth);
}

void SerialPort::onSendCommand(quint8 command, quint8 bit_depth, quint64 address)
{
    if (!m_port || !m_port->isOpen())
    {
        connection_state.first = "Порт закрыт. Команда не отправлена";
        connection_state.second = 3;
        emit ConnectionState(&connection_state);
        return;
    }

    if (address == 0)
    {
        connection_state.first = "Некорректный адрес устройства";
        connection_state.second = 3;
        emit ConnectionState(&connection_state);
        return;
    }

    QByteArray packet;
    // Заголовок
    packet.append(0xAA);
    packet.append(0x55);

    // Команда и значение
    packet.append(command);
    packet.append(bit_depth);

    // Адрес в LittleEndian
    for (int i = 0; i < 8; ++i) {
        packet.append(static_cast<char>((address >> (8*i)) & 0xFF));
    }

    // Отправка
    qint64 written = m_port->write(packet);
    if (written != packet.size())
    {
        connection_state.first = "Ошибка отправки команды";
        connection_state.second = 3;
        emit ConnectionState(&connection_state);
    }
}
