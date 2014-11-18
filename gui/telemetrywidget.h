#pragma once
#include <QWidget>

class QLabel;

/// GUI element to display the contents of the bit-packed telemetry messages
/// and the raw-mode IMU messages.
class TelemetryWidget : public QWidget
{
    Q_OBJECT
public:
    /// Constructor.
    explicit TelemetryWidget(QWidget *parent = 0);

public slots:
    /// Bypass-mode IMU message.
    void bypassImu(int16_t gyroX, int16_t gyroY, int16_t gyroZ,
                   int16_t accX, int16_t accY, int16_t accZ);

    /// Bit-packed telemetry message 22.
    void telemetry1(float roll, float pitch, float yaw, int packetLoss,
                    int rssi, unsigned int throttle, float altPre, int magX,
                    int magY, int magZ, float velN, float velE, float velD,
                    float errN, float errE, float errD, float battHeli,
                    unsigned int flightTime, int svs, int holdMode,
                    int picture, float current);

    /// Bit-packed telemetry message 23.
    void telemetry2(float roll, float pitch, float yaw, int packetLoss,
                    int rssi, unsigned int throttle, float altPre, int altGps,
                    double lat, double lng, float pdop, float hacc, float vacc,
                    int gpsTime, float temperature, unsigned int tilt);

protected:
    /// Acceleration in vehicle frame.
    QLabel *accX;
    QLabel *accY;
    QLabel *accZ;

    /// Altitude ASL (GPS)
    QLabel *alt;

    /// Altitude AGL (barometric)
    QLabel *alt2;

    /// Rotation rate in vehicle frame.
    QLabel *gyroX;
    QLabel *gyroY;
    QLabel *gyroZ;

    /// Magnetic heading.
    QLabel *heading;

    /// Lateral position (GPS).
    QLabel *lat;
    QLabel *lng;

    /// Magnetometer reading in mG.
    QLabel *magX;
    QLabel *magY;
    QLabel *magZ;

    /// Percentage dilution of precision.
    QLabel *pdop;

    /// Orientation.
    QLabel *pitch;
    QLabel *roll;

    /// Velocity in Earth frame.
    QLabel *velD;
    QLabel *velE;
    QLabel *velN;
};
