#pragma once
#include <QHostAddress>
#include <QIODevice>

class QUdpSocket;
/// Uses a thin wrapper over the Draganflyer API (delimiter 0xFF, 0x7E)
/// messages to allow echoing over a network those messages sent and received
/// by an instance of Dragan View and the vehicle it is connected to.
///
/// This class also enables control of the vehicle through Dragan View (in
/// particular, by devices which do not have a XBee dongle but do have network
/// access to a computer or HGCS which does).
class RemoteController : public QIODevice
{
    Q_OBJECT
public:
    /// Constructor.
    ///
    /// @param hostAddress Address of host machine.
    /// @param hostPort UDP port to use on server-side.
    /// @param passive if true controls will not be sent (monitor-only mode).
    /// @param parent Owning QObject.
    explicit RemoteController(QHostAddress hostAddress = QHostAddress::LocalHost,
                           unsigned short hostPort = 65213, bool passive = true,
                           QObject *parent = 0);
signals:
    /// Emitted for every echoed message.
    ///
    /// @param message Message which Dragan View either sent or received.
    /// @param incoming true if this message was received by Dragan View from
    /// a vehicle.
    void message(QByteArray message, bool incoming);

protected:
    /// Return number of bytes ready to be read.
    ///
    /// Implemented only to satisfy pure-virtual from QIODevice, received
    /// messages are reported through message().
    /// @return 0
    qint64 bytesAvailable() const { return 0; }

    /// This QIODevice is sequential.
    /// @return true
    bool isSequential() const { return true; }

    /// Read data from device.
    ///
    /// Implemented only to satisfy pure-virtual from QIODevice.
    /// @param data Unused.
    /// @param maxlen Unused.
    /// @return 0
    qint64 readData(char */*data*/, qint64 /*maxlen*/) { return 0; }

    /// Write data to device.
    ///
    /// @param data Bytes to be written.
    /// @param len Number of bytes to be written.
    /// @return Number of bytes written (not including wrapper).
    qint64 writeData(const char *data, qint64 len);

    /// Network address of server.
    QHostAddress hostAddress;

    /// UDP port on server to connect to.
    unsigned short hostPort;

    /// Passive-mode, if true only subscription messages will be sent.
    bool passive;

    /// Socket used to communicate with server.
    QUdpSocket *socket;

protected slots:
    /// Parse data from socket.
    void onReadyRead();

    /// Send subscription requests.
    void onTimer();
};
