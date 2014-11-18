#pragma once
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <QHostAddress>
#include <QMutex>
#include <QObject>

class QIODevice;
class QTimer;

/// 128-bit key used with TEA to encrypt messages.
uint32_t const teaKey[4]= {
    0x123AFDC6, 0x8704EB2A, 0xBE623A7F, 0x1AD80DF3
};

/// [roll, pitch, throttle, yaw, tilt, ascent, hold, shutter, zoom, aux(1-7)]
/// to [channel0 ... channel15]
const uint8_t txMap[16]= {
    1, 2, 0, 3, 7, 5, 8, 4, 6, 9, 10, 11, 12, 13, 14, 15
};

/// Interface to a DraganflyerAPI vehicle.
///
/// Implements minimal client allowing enumeration, connection, telemetry and
/// control.
class Vehicle : public QObject
{
    Q_OBJECT
public:
    /// Vehicle is at all times in one of these states.
    enum VehicleState {
        IDLE,       ///< Default state; --> {ENUM, CONNECTING}
        ENUM,       ///< Enumerating available vehicles; --> {IDLE}
        CONNECTING, ///< Attempting connection; --> {IDLE, CONNECTED}
        CONNECTED   ///< Active communication with vehicle; --> {IDLE}
    };

    /// Constructor.
    explicit Vehicle(QObject *parent = 0);

    /// Destructor.
    ~Vehicle();

    /// Generate or verify the checksum byte which terminates all XBee packets.
    /// @return complement of the sum modulo 2^8.
    /// @param data address of first byte to include in checksum.
    /// @param length total number of bytes to include in checksum.
    static unsigned char checksum(unsigned char const *data,
                                  unsigned int length);

    /// Generate or verify the cyclic redundancy check which terminates all
    /// Draganflyer API messages.
    ///
    /// Note that for encrypted messages the CRC is calculated from the
    /// unencrypted data.
    /// @return CRC-16-ANSI.
    /// @param data address of the first byte to include in the CRC.
    /// @param length total number of bytes to include in the CRC.
    static uint16_t crc(unsigned char const *data,
                        unsigned int length);

    /// Use Tiny Encryption Algorithm to decrypt data.
    /// @param data pointer to the message to be decrypted.
    /// @param output pointer to buffer where decrypted bytes will be written.
    /// @param key 128-bit encryption key.
    /// @param start offset which is applied to both data and output, used
    /// to skip bytes which should not be decrypted (e.g. message headers).
    /// @param count total number of bytes to decrypt, if this is not a
    /// multiple of 8 then the remaining bytes will be ignored.
    static void decrypt(unsigned char const *data,
                        unsigned char *output,
                        uint32_t const key[],
                        unsigned int start,
                        unsigned int count);

    /// Use Tiny Encryption Algorithm to encrypt data.
    /// @param data pointer to the message to be encrypted.
    /// @param output pointer to buffer where encrypted bytes will be written.
    /// @param key 128-bit encryption key.
    /// @param start offset which is applied to both data and output, used
    /// to skip bytes which should not be encrypted (e.g. message headers).
    /// @param count total number of bytes to encrypt, if this is not a
    /// multiple of 8 then the remaining bytes will be ignored.
    static void encrypt(unsigned char const *data,
                        unsigned char *output,
                        uint32_t const key[],
                        unsigned int start,
                        unsigned int count);

    /// Get current connection state.
    /// @return current connection state of this Vehicle.
    VehicleState getState() const { return state; }

signals:
    /// Results of parsing the bypass-mode sensor message.
    ///
    /// Emitted at 100Hz while connected in wired mode and
    /// Vehicle::config == false.
    void imuChanged(int16_t gyroX,
                    int16_t gyroY,
                    int16_t gyroZ,
                    int16_t accX,
                    int16_t accY,
                    int16_t accZ);

    /// Will be emitted for every outgoing message and every valid incoming
    /// message.
    ///
    /// Incoming messages will be subjected to basic validity tests before
    /// emitting this signal.<BR>
    /// Incoming bytes that are not part of a message will not be emitted.<BR>
    /// Incoming messages which fail checksum and/or CRC (where applicable)
    /// will not be emitted.<BR>
    /// Outgoing messages are always emitted.<BR>
    /// Note that these will be exactly the bytes written to or read from the
    /// serial port (i.e. where applicable these bytes will already/still be
    /// encrypted and/or wrapped).
    /// @param message message sent / received.
    /// @param incoming true if the message originated outside this program.
    void message(QByteArray message,
                 bool incoming);

    /// Will be emitted for every Vehicle::state transition.
    /// @param state current connection state of this Vehicle.
    void stateChanged(Vehicle::VehicleState state);

    /// Results of parsing the bit-packed telemetry message #22.
    ///
    /// Emitted at 5Hz while telemetry streaming is active.
    void telemetry1Changed(float roll,
                           float pitch,
                           float yaw,
                           int packetLoss,
                           int rssi,
                           unsigned int throttle,
                           float altPre,
                           int magX,
                           int magY,
                           int magZ,
                           float velN,
                           float velE,
                           float velD,
                           float errN,
                           float errE,
                           float errD,
                           float battHeli,
                           unsigned int flightTime,
                           int svs,
                           int holdMode,
                           int picture,
                           float current);

    /// Results of parsing the bit-packed telemetry message #23.
    ///
    /// Emitted at 5Hz while telemetry streaming is active.
    void telemetry2Changed(float roll,
                           float pitch,
                           float yaw,
                           int packetLoss,
                           int rssi,
                           unsigned int throttle,
                           float altPre,
                           int altGps,
                           double lat,
                           double lng,
                           float pdop,
                           float hacc,
                           float vacc,
                           int gpsTime,
                           float temperature,
                           unsigned int tilt);

    /// Will be emitted during enumeration, once for every response received.
    /// @param vehicleMac MAC address of the vehicle discovered.
    /// @param channel ZigBee channel the vehicle is using.
    void vehicleFound(uint64_t vehicleMac,
                      uint8_t channel);

public slots:
    /// Send arm command when in bypass-mode.
    void armHeli();

    /// Terminate connection to vehicle.
    ///
    /// Upon return Vehicle::state will be IDLE, and Vehicle::serialPort will
    /// be null.
    void close();

    /// Send disarm command when in bypass mode.
    void disarmHeli();

    /// Enter bypass-mode, enables motor speed control and high frequency IMU
    /// message, disables channel level control and normal telemetry messages.
    void enterBypass();

    /// Use named serial port to send identification requests on all channels.
    ///
    /// @param port name of serial port to use.
    /// Caller must make sure this port is not already in use.
    void enumerate(QString port);

    /// Leave bypass-mode.
    void leaveBypass();

    /// Establish wired communication with a vehicle.
    /// @param port name of the serial port to use.
    /// @param config connect to vehicle in 'config' mode where all bypass
    /// messages are disabled.
    void open(QString port,
              bool config = false);

    /// Establish wireless communication with a vehicle using an XBee module.
    /// @param port name of the serial port to use.
    /// @param vehicleMac MAC address of the target remote vehicle.
    /// @param channel ZigBee channel to use, 0x0C to 0x17 are valid.
    /// @param config connect to vehicle in 'config' mode where controls are
    /// disabled.
    void open(QString port,
              uint64_t vehicleMac,
              uint8_t channel,
              bool config = false);

    /// Establish communication with a vehicle via an instance of Dragan View.
    /// @param hostAddress Address of host running Dragan View.
    /// @param hostPort UDP port on host to use.
    void open(QHostAddress hostAddress,
              quint16 hostUdp);

    /// Set commanded control values.
    ///
    /// In zigbee and wired+non-bypass modes these are taken to be roll, pitch,
    /// throttle, yaw, tilt, ascent, hold, shutter.<BR>
    /// In bypass mode these are motor levels.<BR>
    /// In either case these must be bound to the range [0, 100].
    void setControls(uint8_t c0,
                     uint8_t c1,
                     uint8_t c2,
                     uint8_t c3,
                     uint8_t c4,
                     uint8_t c5,
                     uint8_t c6,
                     uint8_t c7);

    /// Enable or disable the bit-packed telemetry stream.
    ///
    /// @param enable if true telemetry stream will be enabled.
    /// Telemetry stream is not available when connected in bypass-mode.
    void streamTelemetry(bool enable = true);

protected:
    /// Where a message spans multiple read requests this holds the incomplete
    /// remainder.
    QByteArray buffer;

    /// Must be held for all accesses of buffer.
    QMutex bufferMutex;

    /// Current bypass-mode state.
    bool bypassMode;

    /// In zigbee mode this is the channel being communicated on.
    quint8 channel;

    /// Disable bypass-mode (where applicable) and controls.
    ///
    /// If Vehicle::zigbee == true then this simply disables sending controls.
    /// <BR>
    /// If Vehicle::zigbee == false then this disables bypass-mode messages.
    /// <BR>
    bool config;

    /// Connection attempt counter.
    ///
    /// Used to decide when to give up on connecting to a vehicle.
    unsigned int connAttempt;

    /// Commanded control channel values.
    int16_t controls[16];

    /// Control interval counter.
    ///
    /// Constrained to [0 -> 4] and incremented on every control tick.
    unsigned int controlsInterval;

    /// This timer is used to pulse controls and coordinate the skip interval.
    QTimer *controlsTimer;

    /// Enumeration attempt counter.
    ///
    /// Used to decide when to move on to another channel.
    unsigned int enumAttempt;

    /// True if we've received the lower 4 bytes of the local XBee module MAC.
    ///
    /// To ensure a complete local MAC is obtained the low bytes are requested
    /// until received. Then this is set true and the high bytes are requested
    /// until received at which time they are combined.
    bool haveMacLow;

    /// Counter incremented per invokation of Vehicle::timer.
    ///
    /// Used to alternate between sending acquire and query messages during
    /// connection, and to request telemetry messages at 1Hz from inside the
    /// 10Hz timer.
    int iter;

    /// In zigbee mode this is the MAC address of the local device.
    ///
    /// Used only in the acquire message to tell the vehicle which device is
    /// requesting connection.
    uint64_t localMac;

    /// Temporary storing the lower 4 bytes of the local MAC address.
    ///
    /// Needed as the XBee sends high/low as two sepatate messages.
    uint32_t macLowBytes;

    /// Commanded motor speeds
    uint16_t motors[8];

    /// In zigbee mode this is the MAC address of the vehicle connected to.
    uint64_t remoteMac;

    /// Mutex protecting access to Vehicle::serialPort.
    QMutex serialMutex;

    /// VCP device used to send/receive message to/from a vehicle.
    ///
    /// While in this example a QextSerialPort is always used, in fact any
    /// appropriate QIODevice may be used (e.g. a wrapper for USBXpress API).
    QIODevice *serialPort;

    /// Current connection state of the vehicle.
    VehicleState state;

    /// Determines whether the bit-packed telemetry messages #22, #23 should
    /// be streamed.
    ///
    /// If true a telemetry request is sent every second.<BR>
    /// If false a telemtry stop is sent for every telemetry message received
    /// until they actually stop.<BR>
    bool streamingTelemetry;

    /// Throttle mode, legacy or spring-centered.
    ///
    /// Initialized to -1, this is retreived from the vehicle's EEPROM after
    /// connection.<BR>
    /// 0 for legacy [0 -> 1023] and 1 for spring-centered [-511 -> 511].<BR>
    /// Until this has been sucessfully retreived controls are disabled to
    /// avoid accidents.
    int throttleMode;

    /// This timer is used to drive the enumeration and connection procedures.
    QTimer *timer;

    /// Enable wireless communication through a XBee module.
    ///
    /// If true, enumeration is possible, a connection procedure is used to
    /// set channel, send acquire, send query, and get throttle mode.<BR>
    /// All outgoing messages are wrapped in a ZigBee packet, all incoming
    /// messages are assumed to be in ZigBee packets.
    bool zigbee;

protected slots:
    /// Request the MAC address of the local zigbee module.
    /// @param highBytes if true the high 4 bytes of the MAC address will be
    /// requested.
    void getLocalMac(bool highBytes);

    /// Handle a few connection and enum related messages which are not config
    /// messages.
    /// @param message message to parse
    /// @param incoming if false the message will be ignored.
    void onMessage(QByteArray message,
                   bool incoming);

    /// Invoked by the serialPort when data has been received.
    void onReadyRead();

    /// Invoked by the connection / enum timer.
    void onTimer();

    /// Handle message received from networked host.
    ///
    /// @param msg Message received.
    /// @param incoming True if message was received (by the host) from a
    /// vehicle.
    void onUdpMessage(QByteArray msg, bool incoming);

    /// Interpret config messages, decrypt, CRC, act.
    bool parseConfigMessage(QByteArray message);

    /// Wrap message in a ZigBee packet (if applicable) and write it to the
    /// serial port.
    ///
    /// If zigbee == false message is written verbatim to serial port.<BR>
    /// destination defaults to the broadcast address.
    /// @param data data to be sent.
    /// @param destination MAC address of recipient (if applicable).
    void send(QByteArray data,
              uint64_t destination = 0xFFFFULL);

    /// Construct an acquire message and send it to the broadcast address on
    /// the current channel.
    ///
    /// This message is only meaningful when Vehicle::zigbee == true.
    void sendAcquire();

    /// Invoked by the controlTimer and sends either control channels or motor
    /// speeds as appropriate.
    void sendControl();

    /// Send an identify request to broadcast on the current channel.
    void sendEnumRequest();

    /// Send any 0xFF / 'config' type message.
    ///
    /// Please refer to the API spreadsheet regarding message types, subtypes,
    /// modes and payloads. The following summary is incomplete and non-
    /// authoritative, describing only a subset of the real-time, eeprom and
    /// bypass types.
    /// @param type Major type of message.<BR>
    /// 0x1 for real-time, 0x2 for EEPROM.
    /// @param subtype Sub-type of the message.<BR>
    /// For real-time messages this refers to the specific telemetry message
    /// being requested (e.g. 0x16 for bit-packed telemetry message #22).<BR>
    /// For EEPROM messages this refers to the EEPROM section being requested
    /// or written.
    /// @param mode Access mode. Specific meaning depends on both type and
    /// sub-type<BR>
    /// For real-time telemetry messages #22, 23;
    /// 0=Stop, 1=Stream, 2=Momentary<BR>
    /// For EEPROM and bypass-mode motor speed; 0=Read, 1=Write.
    /// @param payload OPTIONAL. Refer to API spreadsheet.
    void sendMessage(uint8_t type,
                     uint8_t subtype,
                     uint8_t mode,
                     QByteArray const &payload = QByteArray());

    /// Construct a query message and send it to the target remote MAC on the
    /// current channel.
    ///
    /// This message is only meaningful when Vehicle::zigbee == true.
    void sendQuery();

    /// In zigbee mode send a channel set message to the zigbee module.
    /// @param channel ZigBee channel to set the XBee module to.
    void setChannel(uint8_t channel);
};
