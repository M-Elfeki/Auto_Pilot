#include "remotecontroller.h"
#include <QtEndian>
#include <QTimer>
#include <QUdpSocket>
#include "vehicle.h"

RemoteController::RemoteController(QHostAddress hostAddress,
                                   unsigned short hostPort,
                                   bool passive,
                                   QObject *parent) :
    QIODevice(parent), hostAddress(hostAddress), hostPort(hostPort),
    passive(passive), socket(new QUdpSocket(this))
{
    QTimer *timer = new QTimer(this);
    connect(socket, SIGNAL(readyRead()), SLOT(onReadyRead()));
    connect(timer, SIGNAL(timeout()), SLOT(onTimer()));
    socket->bind();
    timer->start(400);
}

void RemoteController::onReadyRead()
{
    while (socket->hasPendingDatagrams()) {
        QByteArray bytes;
        bytes.resize(socket->pendingDatagramSize());
        socket->readDatagram(bytes.data(), bytes.length());
        if (bytes[0] != (char)0xDF || bytes.length() < 5) {
            qDebug()<<"rejected delimiter / impossible length";
            return;
        }
        int length = qFromBigEndian<uint16_t>(
                    (unsigned char const *)bytes.constData() + 1);
        if (length + 4 > bytes.length()) {
            qDebug()<<"rejected length"<<(length + 4)<<">"<<bytes.length();
            return;
        }
        if  (Vehicle::checksum(
                 (unsigned char const *)bytes.constData() + 3, length + 1)) {
            qDebug()<<"rejected checksum";
            return;
        }
        quint8 type = bytes[3] & 0xF0;
        quint8 mode = bytes[3] & 0xF;
        switch (type) {
        case 0x00: {
            // Presence broadcast from Dragan View, not used in this example
            // but show it in the message monitor.
            emit message(bytes, true);
            return;
        }
            break;
        case 0x10: {
            // Echoed message.
            QByteArray msg;
            // Strip the network wrapper using inner-message length if it is a
            // known type.
            if (bytes[4] == (char)0x7EU && length >= 4)
                msg = bytes.mid(4, qFromBigEndian<quint16>(
                                    (unsigned char const *)
                                    bytes.constData() + 5) + 4);
            else if ((bytes[4] == (char)0xFFU || bytes[4] == (char)0xFEU) &&
                     length >= 6)
                msg = bytes.mid(4, qFromBigEndian<quint16>(
                                    (unsigned char const *)
                                    bytes.constData() + 6) + 6);
            else
                msg = bytes;

            if (mode == 0x01) // Message was received by Dragan View from a
                emit message(msg, true);                        // vehicle.
            else if (mode == 0x02) // Message was sent by Dragan View to a
                emit message(msg, false);                       // vehicle.
        }
            break;
        default: {
            qDebug()<<"Ignoring unknown message type"<<type;
        }
            break;
        }
    }
}

void RemoteController::onTimer()
{
    QByteArray bytes(5, '\0');
    bytes[0] = (char)0xDFU;
    qToBigEndian<uint16_t>(1, (unsigned char *)bytes.data() + 1);
    bytes[3] = passive? 0x13 : 0x15;
    bytes[4] = Vehicle::checksum(
                (unsigned char const *)bytes.constData() + 3, 1);
    socket->writeDatagram(bytes, hostAddress, hostPort);
}

qint64 RemoteController::writeData(const char *data, qint64 len)
{
    if (passive)
        return 0;
    QByteArray bytes(len + 5, '\0');
    bytes[0] = (char)0xDF;
    qToBigEndian<quint16>(len + 1, (unsigned char *)bytes.data() + 1);
    bytes[3] = 0x12;
    memcpy(bytes.data() + 4, data, len);
    bytes[(int)len+4] = Vehicle::checksum(
                (unsigned char const *)bytes.constData() + 3, len + 1);
    return socket->writeDatagram(bytes, hostAddress, hostPort) - 5;
}
