#include "vehicle.h"
#include <QTimer>
#include <QtEndian>
#include <QDebug>
#include "com/serial/qextserialport.h"
#include "com/remotecontroller.h"

Vehicle::Vehicle(QObject *parent) :
    QObject(parent), buffer(), bufferMutex(), bypassMode(false), channel(0),
    config(false), connAttempt(0), controls(), controlsInterval(0),
    controlsTimer(new QTimer(this)), enumAttempt(0), haveMacLow(false),
    iter(0), localMac(0), macLowBytes(0), motors(), remoteMac(0),
    serialMutex(), serialPort(0), state(IDLE), streamingTelemetry(false),
    throttleMode(-1), timer(new QTimer(this)), zigbee(true)
{
    connect(timer, SIGNAL(timeout()),
            this, SLOT(onTimer()));
    connect(this, SIGNAL(message(QByteArray,bool)),
            this, SLOT(onMessage(QByteArray,bool)));
    connect(controlsTimer, SIGNAL(timeout()),
            this, SLOT(sendControl()));
    timer->start(100);
    controlsTimer->start(20);
}

Vehicle::~Vehicle()
{
    close();
}

void Vehicle::armHeli()
{
    if (!config && !zigbee)
        sendMessage(6, 2, 1);
}

unsigned char Vehicle::checksum(unsigned char const *data, unsigned int length)
{
    unsigned char sum = 0;
    for (unsigned int i = 0; i < length; i++)
        sum += data[i];
    return 0xFFU - sum;
}

void Vehicle::close()
{
    if (state == CONNECTED && !config && !zigbee)
        sendMessage(6, 0, 0);
    bool gotMutex = serialMutex.tryLock(200);
    if (serialPort)
        QMetaObject::invokeMethod(serialPort, "deleteLater", Qt::QueuedConnection);
    serialPort = 0;
    if (gotMutex)
        serialMutex.unlock();
    state = IDLE;
    emit stateChanged(state);
}

uint16_t Vehicle::crc(unsigned char const *input, unsigned int length)
{
    uint16_t CRC = 0xFFFFU;
    bool carry;
    for (unsigned int i = 0; i < length; i++) {
        CRC ^= input[i];
        for (int j = 0; j < 8; j++) {
            carry = CRC & 0x1;
            CRC >>= 1;
            if (carry)
                CRC ^= 0xA001U;
        }
    }
    return CRC;
}

void Vehicle::decrypt(unsigned char const *data, unsigned char *output,
                      uint32_t const key[], uint32_t skip, uint32_t count)
{
    uint32_t delta = 0x9E3779B9, num_rounds = 32, sum = delta * num_rounds;
    int32_t v[2];
    for (uint32_t i = 0; i < (count >> 3); i++, sum = delta * num_rounds) {
        memcpy(v, data + skip + (i << 3), 8);
        for (uint32_t n = 0; n < num_rounds; n++) {
            v[1] -= ((v[0] << 4 ^ v[0] >> 5) + v[0]) ^
                    (sum + key[(sum >> 11) & 3]);
            sum -= delta;
            v[0] -= ((v[1] << 4 ^ v[1] >> 5) + v[1]) ^ (sum + key[sum & 3]);
        }
        memcpy(output + skip + (i << 3), v, 8);
    }
}

void Vehicle::disarmHeli()
{
    if (!config && !zigbee)
        sendMessage(6, 3, 1);
}

void Vehicle::encrypt(unsigned char const *data, unsigned char *output,
                      uint32_t const key[], uint32_t skip, uint32_t count)
{
    uint32_t delta = 0x9E3779B9, num_rounds = 32, sum = 0;
    int32_t v[2];
    for (uint32_t i = 0; i < (count >> 3); i++, sum = 0) {
        memcpy(v, data + skip + (i << 3), 8);
        for (uint32_t n = 0; n < num_rounds; n++) {
            v[0] += ((v[1] << 4 ^ v[1] >> 5) + v[1]) ^ (sum + key[sum & 3]);
            sum += delta;
            v[1] += ((v[0] << 4 ^ v[0] >> 5) + v[0]) ^
                    (sum + key[(sum >> 11) & 3]);
        }
        memcpy(output + skip + (i << 3), v, 8);
    }
}

void Vehicle::enterBypass()
{
    if (!config && !zigbee) {
        sendMessage(6, 0, 1);
        bypassMode = true;
    }
}

void Vehicle::enumerate(QString port)
{
    QMutexLocker locker(&serialMutex);
    if (state == IDLE) {
        zigbee = true;
        QextSerialPort *tempPort = new QextSerialPort(port);
        tempPort->setBaudRate(BAUD57600);
        tempPort->setParity(PAR_NONE);
        tempPort->setDataBits(DATA_8);
        tempPort->setStopBits(STOP_1);
        tempPort->setFlowControl(FLOW_OFF);
        if (tempPort->open(QIODevice::ReadWrite)) {
            tempPort->setDtr(false);
            connect(tempPort, SIGNAL(readyRead()),
                    this, SLOT(onReadyRead()));
            serialPort = tempPort;
            channel = 0xC; // first valid channel
            enumAttempt = 0;
            state = ENUM;
            emit stateChanged(state);
            locker.unlock();
            setChannel(channel);
        } else {
            delete tempPort;
        }
    }
}

void Vehicle::getLocalMac(bool highBytes)
{
    if (!zigbee)
        return;
    QByteArray packet = QByteArray(8, '\0');
    packet[0] = 0x7E;
    packet[1] = 0x0;
    packet[2] = 0x4;
    packet[3] = 0x8;
    packet[4] = 0x1;
    packet[5] = 'S';
    packet[6] = highBytes? 'H' : 'L';
    packet[7] = checksum((unsigned char const *)packet.constData() + 3, 4);
    QMutexLocker locker(&serialMutex);
    if (serialPort) {
        serialPort->write(packet);
        emit message(packet, false);
    }
}

void Vehicle::leaveBypass()
{
    if (!config && !zigbee) {
        sendMessage(6, 0, 0);
        bypassMode = false;
    }
}

void Vehicle::onMessage(QByteArray message, bool incoming)
{
    unsigned char const *data = (unsigned char const *)message.constData();
    if (incoming && data[0] == 0x7E) {
        uint8_t type = data[3];
        if (type == 0x80) { // XBee API 64-bit addressed receive
            uint64_t sourceMac = qFromBigEndian<quint64>(data + 4);
            if (state == CONNECTING && sourceMac == remoteMac &&
                    data[14] == 0x1) { // Query response
                state = CONNECTED;
                emit stateChanged(CONNECTED);
            }
            if (state == CONNECTED && sourceMac == remoteMac &&
                    data[14] == 0x3) { // Alarm
                if (data[28]) { // Ack is required
                    QByteArray response(4, 0x0);
                    response[0] = 4;
                    response[1] = 1;
                    qToLittleEndian(crc((unsigned char const *)
                                        response.constData(), 2),
                                    (unsigned char *)(response.data() + 2));
                    send(response, remoteMac);
                }
            }
            if (state == ENUM && data[14] == 0xF8) // Enumeration response
                emit vehicleFound(sourceMac, data[31] + 0xC);
        } else if (type == 0x88 && data[7] == 0) { // Module configuration
            if (message[5] == 'S' && message[6] == 'H' && haveMacLow) {
                // High 4 bytes of local MAC address
                uint64_t macHigh = qFromBigEndian<uint32_t>(
                            (unsigned char const *)message.constData() + 8);
                localMac = (macHigh << 32) | macLowBytes;
            } else if (message[5] == 'S' && message[6] == 'L') {
                // Low 4 bytes of local MAC address
                macLowBytes = qFromBigEndian<uint32_t>(
                            (unsigned char const *)message.constData() + 8);
                haveMacLow = true;
            }
        } else if (type == 0x8A && data[4] == 0) // XBee reset, set channel
            setChannel(channel);
    }
}

void Vehicle::onReadyRead()
{
    QMutexLocker bufferLocker(&bufferMutex);
    QMutexLocker serialLocker(&serialMutex);
    if (serialPort == 0)
        return;
    buffer.append(serialPort->read(serialPort->bytesAvailable()));
    serialLocker.unlock();
    while (buffer.length() >= (zigbee? 5 : 6)) {
        unsigned char const *data = (unsigned char const *)buffer.constData();
        if ((zigbee && data[0] == 0x7E) ||
                (!zigbee && (data[0] == 0xFF || data[0] == 0xFE))) {
            uint16_t length = qFromBigEndian<uint16_t>(data + (zigbee? 1 : 2));
            if (zigbee) {
                if (length == 0 || length > 95) {
                    // Invalid length
                    buffer.remove(0, 1);
                    continue;
                }
                if (length + 4 > buffer.length()) {
                    // Length appears reasonable but exceeds bytes available.
                    // Leave buffer unmodified and return waiting for remainder
                    // of packet to arrive.
                    return;
                }
                if (checksum(data + 3, length + 1)) {
                    // Checksum failed
                    buffer.remove(0, 1);
                    continue;
                }
                if (data[3] == 0x80 &&
                    qFromBigEndian<quint64>(data + 4) == remoteMac &&
                    data[14] == 0xFF &&
                    !parseConfigMessage(
                            QByteArray((char const *)(data + 14),
                                       length - 11))) {
                    // Was a config message, but parsing failed.
                    buffer.remove(0, 1);
                    continue;
                }
                // Appears to be a valid message.
                emit message(QByteArray((char const *)data, length + 4), true);
                buffer.remove(0, length + 4);
            } else {
                // In wired mode the maximum length is effectively the range of
                // an unsigned 16-bit int. However, 200 is longer than any
                // actual message in case a bad message has unreasonable length
                // we don't want to wait for all of it to arrive before
                // continuing.
                if (length > 200) {
                    buffer.remove(0, 1);
                    continue;
                }
                // Length is reasonable but incomplete, wait for the remainder.
                if (length + 6 > buffer.length()) {
                    return;
                }
                QByteArray newMessage((char const *)data, length + 6);
                if (parseConfigMessage(newMessage)) { // Valid message
                    emit message(newMessage, true);
                    buffer.remove(0, length + 6);
                    continue;
                } else { // Parsing failed
                    buffer.remove(0, 1);
                    continue;
                }
            }
        } else {
            // Buffer does not start with a valid delimiter.
            // Trim to the first delimiter found, or clear the buffer if none.
            int delim = -1;
            if (zigbee) {
                delim = buffer.indexOf(0x7E);
            } else {
                int FE_delim = buffer.indexOf((char)0xFE);
                int FF_delim = buffer.indexOf((char)0xFF);
                if (FF_delim < 0 || FE_delim < FF_delim)
                    delim = FE_delim;
                else
                    delim = FF_delim;
            }
            if (delim > 0)
                buffer.remove(0, delim);
            else
                buffer.clear();
        }
    }
}

void Vehicle::onTimer()
{
    iter++;
    switch (this->state) {
    case IDLE:
        break;
    case ENUM:
    {
        if (enumAttempt++ % 3 == 2) {
            if (++channel > 0x17)
                close();
            else
                setChannel(channel);
        } else {
            sendEnumRequest();
        }
    }
        break;
    case CONNECTING:
    {
        if (connAttempt++ > 100) {
            close();
        } else if (zigbee) {
            if (localMac == 0) {
                getLocalMac(haveMacLow);
            } else {
                if (iter % 2 == 0)
                    sendAcquire();
                // Queries are only sent after several acquires have been sent,
                // this is to allow the present program to switch between
                // config-only and master modes. Without this delay it would be
                // possible for the vehicle to miss the acquire and interpret
                // the subsequent query as a request to reconnect in the
                // previous mode.
                else if (connAttempt > 10)
                    sendQuery();
            }
        } else { // Wired mode
            if (config) {
                // Wired, non-bypass mode requires no connection procedure.
                state = CONNECTED;
                emit stateChanged(CONNECTED);
            } else {
                state = CONNECTED;
                emit stateChanged(CONNECTED);
                // Set bypass mode
                //sendMessage(6, 0, 1);
            }
        }
    }
        break;
    case CONNECTED:
    {
        // Connected but need throttle mode before controls can be enabled.
        if (throttleMode < 0 && (zigbee || config))
            sendMessage(2, 16, 0);
        // Bit-packed telemetry messages need to be requested periodically or
        // they time-out.
        if (streamingTelemetry && (iter % 10 == 0))
            sendMessage(1, 22, 1);
    }
        break;
    }
}

void Vehicle::onUdpMessage(QByteArray msg, bool incoming)
{
    if (!incoming)
        return;
    unsigned char *data = (unsigned char *)msg.data();
    if (data[0] == 0xFF)
        parseConfigMessage(msg);
    else if (data[0] == 0x7E && data[3] == 0x80 && data[14] == 0xFF)
        parseConfigMessage(
                QByteArray((char const *)(data + 14),
                           msg.length() - 15));
    else
        return;
    emit message(msg, incoming);
}

void Vehicle::open(QString port, bool config)
{
    QMutexLocker locker(&serialMutex);
    if (state != IDLE)
        return;
    zigbee = false;
    bypassMode = false;
    this->config = config;
    QextSerialPort *tempPort = new QextSerialPort(port);
    tempPort->setBaudRate(BAUD115200);
    tempPort->setParity(PAR_NONE);
    tempPort->setDataBits(DATA_8);
    tempPort->setStopBits(STOP_1);
    tempPort->setFlowControl(FLOW_OFF);
    if (tempPort->open(QIODevice::ReadWrite)) {
        tempPort->setDtr(true);
        serialPort = tempPort;
        connect(tempPort, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
        connAttempt = 0;
        state = CONNECTING;
        emit stateChanged(CONNECTING);
        locker.unlock();
        //if (!config) // Try to set bypass-mode
        //    sendMessage(6, 0, 1);
    } else {
        delete tempPort;
    }
}

void Vehicle::open(QString port, uint64_t vehicleMac, uint8_t channel, bool config)
{
    QMutexLocker locker(&serialMutex);
    if (state != IDLE)
        return;
    remoteMac = vehicleMac;
    this->channel = channel;
    zigbee = true;
    this->config = config;
    QextSerialPort *tempPort = new QextSerialPort(port);
    tempPort->setBaudRate(BAUD57600);
    tempPort->setParity(PAR_NONE);
    tempPort->setDataBits(DATA_8);
    tempPort->setStopBits(STOP_1);
    tempPort->setFlowControl(FLOW_OFF);
    if (tempPort->open(QIODevice::ReadWrite)) {
        tempPort->setDtr(false);
        serialPort = tempPort;
        connect(tempPort, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
        connAttempt = 0;
        localMac = 0;
        haveMacLow = false;
        state = CONNECTING;
        emit stateChanged(CONNECTING);
        locker.unlock();
        setChannel(channel);
    } else {
        delete tempPort;
    }
}

void Vehicle::open(QHostAddress hostAddress, quint16 hostUdp)
{
    qDebug()<<"remote open";
    QMutexLocker locker(&serialMutex);
    if (state != IDLE)
        return;
    zigbee = false;
    this->config = true;
    RemoteController *tempPort = new RemoteController(hostAddress, hostUdp, false, this);
    if (tempPort->open(QIODevice::ReadWrite)) {
        serialPort = tempPort;
        connect(tempPort, SIGNAL(message(QByteArray,bool)), this, SLOT(onUdpMessage(QByteArray,bool)));
        connAttempt = 0;
        state = CONNECTING;
        emit stateChanged(CONNECTING);
        locker.unlock();
    } else {
        delete tempPort;
    }
}

bool Vehicle::parseConfigMessage(QByteArray message)
{
    uint16_t length = qFromBigEndian<uint16_t>(
                (unsigned char const *)(message.constData() + 2));
    if (length + 6 != message.length())
        return false;
    // All 0xFF mesage types except 0x6 are encrypted.
    if (((unsigned char)message.at(1) != 0x6) &&
            ((unsigned char)message.at(1) != 0xA)) {
        decrypt((unsigned char const *)message.constData(),
                (unsigned char *)message.data(), teaKey, 4, length);
    }
    unsigned char const *data = (unsigned char const *)message.constData();
    if (crc(data + 1, length + 5) != 0)
        return false;
    if (data[1] == 0x1) { // Realtime
        if (data[4] == 22) { // Bit-packed telemetry 1
            if (!streamingTelemetry)
                sendMessage(1, 22, 0); // Send a stop request if we don't want
            else {
                int32_t itemp;
                int16_t stemp;
                uint16_t ustemp;
                uint8_t btemp;
                itemp = qFromLittleEndian<int32_t>(data + 5);
                stemp = itemp & UINT16_C(0x7FF);
                if (stemp & UINT16_C(0x400))
                    stemp |= UINT16_C(0xF800);
                float roll = stemp / 10.0f;
                stemp = (itemp & UINT32_C(0x3FF800)) >> 11;
                if ((stemp & UINT16_C(0x400)) != 0)
                    stemp |= UINT16_C(0xF800);
                float pitch = stemp / 10.0f;
                stemp = (itemp & UINT32_C(0xFFC00000)) >> 22;
                if (stemp & UINT16_C(0x200))
                    stemp |= UINT16_C(0xFC00);
                float yaw = stemp;
                int packetLoss = data[9];
                int rssi = data[10];
                uint16_t throttle = qFromLittleEndian<uint16_t>(data + 11);
                float altPre = qFromLittleEndian<int16_t>(data + 13) / 10.0f;
                itemp = qFromLittleEndian<int32_t>(data + 15);
                btemp = data[19];
                stemp = itemp & UINT16_C(0x1FFF);
                if (stemp & UINT16_C(0x1000))
                    stemp |= UINT16_C(0xE000);
                int16_t magX = stemp;
                stemp = (itemp & UINT32_C(0x3FFE000)) >> 13;
                if (stemp & UINT16_C(0x1000))
                    stemp |= UINT16_C(0xE000);
                int16_t magY = stemp;
                stemp = (itemp & UINT32_C(0xFC000000)) >> 26;
                stemp |= btemp << 6;
                if (stemp & UINT16_C(0x1000))
                    stemp |= UINT16_C(0xE000);
                int16_t magZ = stemp;
                itemp = qFromLittleEndian<int32_t>(data + 20);
                stemp = itemp & UINT16_C(0x3FF);
                if (stemp & UINT16_C(0x200))
                    stemp |= UINT16_C(0xFC00);
                float veln = stemp / 10.0f;
                stemp = (itemp & UINT32_C(0xFFC00)) >> 10;
                if (stemp & UINT16_C(0x200))
                    stemp |= UINT16_C(0xFC00);
                float vele = stemp / 10.0f;
                stemp = (itemp & UINT32_C(0x3FF00000)) >> 20;
                if (stemp & UINT16_C(0x200))
                    stemp |= UINT16_C(0xFC00);
                float veld = stemp / 10.0f;
                itemp = qFromLittleEndian<int32_t>(data + 24);
                stemp = itemp & UINT16_C(0x3FF);
                if (stemp & UINT16_C(0x200))
                    stemp |= UINT16_C(0xFC00);
                float errn = stemp;
                if ((itemp & 1 << 30) == 0)
                    errn /= 10;
                stemp = (itemp & UINT32_C(0xFFC00)) >> 10;
                if (stemp & UINT16_C(0x200))
                    stemp |= UINT16_C(0xFC00);
                float erre = stemp;
                if ((itemp & 1 << 30) == 0)
                    erre /= 10;
                stemp = (itemp & UINT32_C(0x3FF00000)) >> 20;
                if (stemp & UINT16_C(0x200))
                    stemp |= UINT16_C(0xFC00);
                float errd = stemp;
                if ((itemp & 1 << 31) == 0)
                    errd /= 10;
                float battHeli = data[28] / 10.0f;
                ustemp = qFromLittleEndian<uint16_t>(data + 29);
                uint32_t timeFlight = ustemp * 40;
                int svs = (data[31] & 0x1F);
                int holdMode = (data[31] & UINT8_C(0xE0)) >> 5;
                float current = data[32] / 10.0f;
                int picture = data[33];
                emit telemetry1Changed(roll, pitch, yaw, packetLoss, rssi,
                                       throttle, altPre, magX, magY, magZ,
                                       veln, vele, veld, errn, erre, errd,
                                       battHeli, timeFlight, svs, holdMode,
                                       picture, current);
            }
        }
        if (data[4] == 23) { // Bit-packed telemetry 2
            if (!streamingTelemetry)
                sendMessage(1, 22, 0);
            else {
                int32_t itemp, itemp2;
                int16_t stemp;
                double dtemp;
                itemp = qFromLittleEndian<int32_t>(data + 5);
                stemp = itemp & UINT16_C(0x7FF);
                if (stemp & UINT16_C(0x400))
                    stemp |= UINT16_C(0xF800);
                float roll = stemp / 10.0f;
                stemp = (itemp & UINT32_C(0x3FF800)) >> 11;
                if ((stemp & UINT16_C(0x400)) != 0)
                    stemp |= UINT16_C(0xF800);
                float pitch = stemp / 10.0f;
                stemp = (itemp & UINT32_C(0xFFC00000)) >> 22;
                if (stemp & UINT16_C(0x200))
                    stemp |= UINT16_C(0xFC00);
                float yaw = stemp;
                int packetLoss = data[9];
                int rssi = data[10];
                uint16_t throttle = qFromLittleEndian<uint16_t>(data + 11);
                float altPre = qFromLittleEndian<int16_t>(data + 13) / 10.0f;
                int16_t altGps = qFromLittleEndian<int16_t>(data + 15);
                itemp = qFromLittleEndian<int32_t>(data + 17);
                itemp2 = itemp & UINT32_C(0x7FFFFF);
                stemp = (itemp & UINT32_C(0xFF800000)) >> 23;
                if (stemp & UINT16_C(0x100))
                    stemp |= UINT16_C(0xFE00);
                dtemp = stemp;
                if (dtemp < 0)
                    dtemp -= itemp2 / 1000000.0;
                else
                    dtemp += itemp2 / 1000000.0;
                double lat = dtemp;
                itemp = qFromLittleEndian<int32_t>(data + 21);
                itemp2 = itemp & 0x007FFFFF;
                stemp = (short)((itemp & 0xFF800000) >> 23);
                if ((stemp & 0x100) != 0)
                    stemp |= (short)0xFE00;
                dtemp = (double)stemp;
                if (dtemp < 0)
                    dtemp -= itemp2 / 1000000.0;
                else
                    dtemp += itemp2 / 1000000.0;
                double lng = dtemp;
                itemp = qFromLittleEndian<int32_t>(data + 25);
                stemp = itemp & UINT16_C(0x3FF);
                float pdop = stemp / 10.0f;
                stemp = itemp >> 10 & UINT16_C(0x7FF);
                float hacc = stemp / 10.0f;
                stemp = (short)((itemp >> 21) & 0x7FF);
                float vacc = ((float)stemp) / 10;
                itemp = qFromLittleEndian<int32_t>(data + 29);
                int32_t timeGps = (itemp & UINT32_C(0xFFFFF)) * 1000;
                float temperature = -1000000;
                stemp = itemp >> 20 & UINT16_C(0xFFF);
                if (stemp & UINT16_C(0x800))
                    stemp |= UINT16_C(0xF000);
                if (stemp != UINT16_C(0x7FF))
                    temperature = stemp * 0.0625f;
                uint8_t tilt = data[33];
                emit telemetry2Changed(roll, pitch, yaw, packetLoss, rssi,
                                       throttle, altPre, altGps, lat, lng,
                                       pdop, hacc, vacc, timeGps, temperature,
                                       tilt);
            }
        }
    }
    if (data[1] == 0x2) { // EEPROM
        if (data[4] == 16)
            throttleMode = data[5] & 0x1;
    }
    if (data[1] == 0x6 && data[4] == 0 && !zigbee && !config) {
        // Bypass-mode IMU readings
        //if (state == CONNECTING) {
        //    state = CONNECTED;
        //    emit stateChanged(CONNECTED);
        //    sendMessage(6, 2, 1); // Arm vehicle
        //}
        int16_t imu[6];
        memcpy(imu, data + 5, sizeof(imu));
        for (int i = 0; i < 6; i++)
            imu[i] = qFromLittleEndian(imu[i]);
        emit imuChanged(imu[0], imu[1], imu[2], imu[3], imu[4], imu[5]);
    }
    return true;
}

void Vehicle::send(QByteArray data, uint64_t destination)
{
    // data must be a valid message, CRC'd and if necessary encrypted.
    if (zigbee) {
        // Wrap message in a XBee packet.
        int length = data.length() + 11;
        unsigned char bytes[14];
        bytes[0] = 0x7E;
        qToBigEndian<uint16_t>(length, bytes + 1);
        bytes[3] = 0x00; // 64-bit addressed transmit
        bytes[4] = 0x00; // Frame #, unused
        qToBigEndian<uint64_t>(destination, bytes + 5);
        bytes[13] = 0x01; // Options, sp. 'no ack'
        data.prepend((char *)bytes, 14);
        data.append(checksum((unsigned char const *)data.constData() + 3,
                    length));
    } else {
        // No additional wrapper needed.
    }
    QMutexLocker locker(&serialMutex);
    if (serialPort) {
        serialPort->write(data);
        locker.unlock();
        emit message(data, false);
    }
}

void Vehicle::sendAcquire()
{
    unsigned char bytes[11];
    bytes[0] = config? 254 : 0;
    qToLittleEndian(localMac, bytes + 1);
    qToLittleEndian(crc(bytes, 9), bytes + 9);
    send(QByteArray((char const *)bytes, 11));
}

void Vehicle::sendControl()
{
    if (state != CONNECTED)
        return;
    if (zigbee && !config) {
        controlsInterval++;
        controlsInterval = controlsInterval % 5;
        if (controlsInterval == 4)
            return;
        uint chCount = 6 + (controlsInterval % 2);
        QByteArray message = QByteArray(4 + chCount * 2, 0x1);
        message[0] = 0x7;
        message[1] = chCount;
        // Controls are not sent when interval == 4, indicate to vehicle on
        // last message before skip that it should use the next few ms to send
        // any message it has queued.
        if (controlsInterval == 3)
            message[1] = message[1] | (1 << 7);
        // Roll, pitch, throttle, and yaw are sent every time.
        for (int i = 0; i < 4; i++) {
            qToBigEndian(controls[i],
                         (unsigned char *)(message.data() + 2 + 2 * i));
            // Mask the value and OR with its index.
            message[2 + 2 * i] = (message[2 + 2 * i] & 0x0F) | (i << 4);
        }
        if (controlsInterval % 2 == 0) { // Shutter and ascent for even
            qToBigEndian(controls[4],
                         (unsigned char *)(message.data() + 2 + 2 * 4));
            qToBigEndian(controls[5],
                         (unsigned char *)(message.data() + 2 + 2 * 5));
            // Mask values and OR with indices.
            message[2 + 2 * 4] = (message[2 + 2 * 4] & 0xF) | (4 << 4);
            message[2 + 2 * 5] = (message[2 + 2 * 5] & 0xF) | (5 << 4);
        } else { // Zoom, tilt, hold for odd
            qToBigEndian(controls[6],
                         (unsigned char *)(message.data() + 2 + 2 * 4));
            qToBigEndian(controls[7],
                         (unsigned char *)(message.data() + 2 + 2 * 5));
            qToBigEndian(controls[8],
                         (unsigned char *)(message.data() + 2 + 2 * 6));
            // Mask values and OR with indices.
            message[2 + 2 * 4] = (message[2 + 2 * 4] & 0xF) | (6 << 4);
            message[2 + 2 * 5] = (message[2 + 2 * 5] & 0xF) | (7 << 4);
            message[2 + 2 * 6] = (message[2 + 2 * 6] & 0xF) | (8 << 4);
        }
        qToLittleEndian(crc((unsigned char const *)
                            message.constData(), 2 + chCount * 2),
                        (unsigned char *)(message.data() + 2 + chCount * 2));
        send(message, remoteMac);
    } else if (!zigbee && !config && bypassMode) {
        uchar ms[16];
        for (int i = 0; i < 8; i++)
            qToLittleEndian<uint16_t>(motors[i], ms + i * 2);
        sendMessage(6, 1, 1, QByteArray((char *)ms, 16));
    } else if (config) {
        uchar data[33];
        data[0] = 10;
        for (int i = 0; i < 10; i++) {
            qToLittleEndian(controls[i], data + 1 + 2 * i);
            // Mask the value and OR with its index.
            data[2 + 2 * i] = (data[2 + 2 * i] & 0x0F) | (i << 4);
        }
        sendMessage(5, 0, 1, QByteArray((char *)data, 21));
    }
}

void Vehicle::sendEnumRequest()
{
    uchar bytes[4];
    bytes[0] = 0xF8; // Identify request
    bytes[1] = 0x0;  // Omit no vehicles
    qToLittleEndian(crc(bytes, 2), bytes + 2);
    send(QByteArray((char*)bytes, 4));
}

void Vehicle::sendMessage(uint8_t type, uint8_t subType, uint8_t mode,
                          QByteArray const &payload)
{
    unsigned char bytes[1024];
    if (state == IDLE)
        return;
    uint16_t length = payload.length() + 2;
    length = ((length % 8) == 0)? length : ((length >> 3) + 1) << 3;
    if (length + 6U > sizeof(bytes))
        return;
    bytes[0] = 0xFF;
    bytes[1] = type;
    qToBigEndian<uint16_t>(length, bytes + 2);
    bytes[4] = subType;
    bytes[5] = mode;
    // Payload
    for (int i = 0; i < payload.length(); i++)
        bytes[6 + i] = payload.constData()[i];
    // Padding
    for (int i = payload.length(); i < length - 2; i++)
        bytes[6 + i] = 0;
    qToLittleEndian(crc(bytes + 1, length + 3), bytes + length + 4);
    if (type != 6)
        encrypt(bytes, bytes, teaKey, 4, length);
    int outLen = length + 6;
    int offset = 0;
    // If message is longer than maximum XBee packet, break it up.
    while (zigbee && outLen > 85) {
        send(QByteArray((char *)(bytes + offset), 85), remoteMac);
        outLen -= 85;
        offset += 85;
    }
    send(QByteArray((char *)(bytes + offset), outLen), remoteMac);
}

void Vehicle::sendQuery()
{
    QByteArray message = QByteArray(3, 0x0);
    message[0] = 1;
    qToLittleEndian(crc((unsigned char const *)message.constData(), 1),
                    (unsigned char *)message.data() + 1);
    send(message, remoteMac);
}

void Vehicle::setChannel(uint8_t channel)
{
    if (!zigbee)
        return;
    QByteArray packet(9, '\0');
    packet[0] = 0x7E;
    packet[1] = 0x00;
    packet[2] = 0x05;
    packet[3] = 0x08; // AT command
    packet[4] = 0x00; // frame #, unused
    packet[5] = 'C';  // "CH" = channel
    packet[6] = 'H';
    packet[7] = channel;
    packet[8] = checksum((unsigned char const *)packet.constData() + 3, 5);
    QMutexLocker locker(&serialMutex);
    if (serialPort) {
        this->channel = channel;
        serialPort->write(packet);
        emit message(packet, false);
    }
}

void Vehicle::setControls(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3,
                          uint8_t c4, uint8_t c5, uint8_t c6, uint8_t c7)
{
    if (throttleMode >= 0 && (zigbee || config)) {
        controls[txMap[0]] = 1022.0*c0/100.0-511;
        controls[txMap[1]] = 1022.0*c1/100.0-511;
        controls[txMap[2]] = 1022.0*c2/100.0-(throttleMode? 511 : 0);
        controls[txMap[3]] = 1022.0*c3/100.0-511;
        controls[txMap[4]] = 1022.0*c4/100.0-511;
        controls[txMap[5]] = 1022.0*c5/100.0-511;
        controls[txMap[6]] = 511.0*c6/100.0;
        controls[txMap[7]] = 1022.0*c7/100.0-511;
        for (int i = 8; i < 16; i++)
            controls[txMap[i]] = 0;
    } else if (!zigbee && !config) {
        motors[0] = 1023*c0/100;
        motors[1] = 1023*c1/100;
        motors[2] = 1023*c2/100;
        motors[3] = 1023*c3/100;
        motors[4] = 1023*c4/100;
        motors[5] = 1023*c5/100;
        motors[6] = 1023*c6/100;
        motors[7] = 1023*c7/100;
    }
}

void Vehicle::streamTelemetry(bool enable)
{
    streamingTelemetry = enable;
    sendMessage(1, 22, enable? 1 : 0);
}
