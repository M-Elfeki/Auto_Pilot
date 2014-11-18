#include "telemetrywidget.h"
#include <QGridLayout>
#include <QLabel>

TelemetryWidget::TelemetryWidget(QWidget *parent) :
    QWidget(parent),
    accX(new QLabel("~", this)),
    accY(new QLabel("~", this)),
    accZ(new QLabel("~", this)),
    alt(new QLabel("~", this)),
    alt2(new QLabel("~", this)),
    gyroX(new QLabel("~", this)),
    gyroY(new QLabel("~", this)),
    gyroZ(new QLabel("~", this)),
    heading(new QLabel("~", this)),
    lat(new QLabel("~", this)),
    lng(new QLabel("~", this)),
    magX(new QLabel("~", this)),
    magY(new QLabel("~", this)),
    magZ(new QLabel("~", this)),
    pdop(new QLabel("~", this)),
    pitch(new QLabel("~", this)),
    roll(new QLabel("~", this)),
    velD(new QLabel("~", this)),
    velE(new QLabel("~", this)),
    velN(new QLabel("~", this))
{
    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(new QLabel("X", this), 0, 1);
    layout->addWidget(new QLabel("Y", this), 0, 2);
    layout->addWidget(new QLabel("Z", this), 0, 3);
    layout->addWidget(new QLabel("Att", this), 1, 0);
    layout->addWidget(new QLabel("Mag", this), 2, 0);
    layout->addWidget(new QLabel("Acc", this), 3, 0);
    layout->addWidget(new QLabel("Gyro", this), 4, 0);
    layout->addWidget(new QLabel("Lat", this), 0, 4);
    layout->addWidget(new QLabel("Lng", this), 1, 4);
    layout->addWidget(new QLabel("VelN", this), 2, 4);
    layout->addWidget(new QLabel("VelE", this), 2, 6);
    layout->addWidget(new QLabel("VelD", this), 3, 4);
    layout->addWidget(new QLabel("PDOP", this), 3, 6);
    layout->addWidget(new QLabel("Alt", this), 4, 4);
    layout->addWidget(new QLabel("Alt2", this), 4, 6);

    layout->addWidget(roll, 1, 1);
    layout->addWidget(pitch, 1, 2);
    layout->addWidget(heading, 1, 3);
    layout->addWidget(magX, 2, 1);
    layout->addWidget(magY, 2, 2);
    layout->addWidget(magZ, 2, 3);
    layout->addWidget(accX, 3, 1);
    layout->addWidget(accY, 3, 2);
    layout->addWidget(accZ, 3, 3);
    layout->addWidget(gyroX, 4, 1);
    layout->addWidget(gyroY, 4, 2);
    layout->addWidget(gyroZ, 4, 3);
    layout->addWidget(lat, 0, 5, 1, 3);
    layout->addWidget(lng, 1, 5, 1, 3);
    layout->addWidget(velN, 2, 5);
    layout->addWidget(velE, 2, 7);
    layout->addWidget(velD, 3, 5);
    layout->addWidget(pdop, 3, 7);
    layout->addWidget(alt, 4, 5);
    layout->addWidget(alt2, 4, 7);
}

void TelemetryWidget::bypassImu(qint16 gyroX, qint16 gyroY, qint16 gyroZ,
                                qint16 accX, qint16 accY, qint16 accZ)
{
    this->gyroX->setNum(gyroX);
    this->gyroY->setNum(gyroY);
    this->gyroZ->setNum(gyroZ);
    this->accX->setNum(accX);
    this->accY->setNum(accY);
    this->accZ->setNum(accZ);
}

void TelemetryWidget::telemetry1(float roll, float pitch, float yaw,
                                 int packetLoss, int rssi,
                                 unsigned int throttle, float altPre, int magX,
                                 int magY, int magZ, float velN, float velE,
                                 float velD, float errN, float errE,
                                 float errD, float battHeli,
                                 unsigned int flightTime, int svs,
                                 int holdMode, int picture, float current)
{
    (void)packetLoss;
    (void)rssi;
    (void)throttle;
    (void)errN;
    (void)errE;
    (void)errD;
    (void)battHeli;
    (void)flightTime;
    (void)svs;
    (void)holdMode;
    (void)picture;
    (void)current;
    this->roll->setText(QString::number(roll, 'f', 1));
    this->pitch->setText(QString::number(pitch, 'f', 1));
    this->heading->setText(QString::number(yaw, 'f', 1));
    this->alt2->setText(QString::number(altPre, 'f', 1));
    this->velE->setText(QString::number(velE, 'f', 1));
    this->velN->setText(QString::number(velN, 'f', 1));
    this->velD->setText(QString::number(velD, 'f', 1));
    this->magX->setText(QString::number(magX, 'f', 1));
    this->magY->setText(QString::number(magY, 'f', 1));
    this->magZ->setText(QString::number(magZ, 'f', 1));
}

void TelemetryWidget::telemetry2(float roll, float pitch, float yaw,
                                 int packetLoss, int rssi,
                                 unsigned int throttle, float altPre,
                                 int altGps, double lat, double lng,
                                 float pdop, float hacc, float vacc,
                                 int gpsTime, float temperature,
                                 unsigned int tilt)
{
    (void)packetLoss;
    (void)rssi;
    (void)throttle;
    (void)hacc;
    (void)vacc;
    (void)gpsTime;
    (void)temperature;
    (void)tilt;
    this->roll->setText(QString::number(roll, 'f', 1));
    this->pitch->setText(QString::number(pitch, 'f', 1));
    this->heading->setText(QString::number(yaw, 'f', 1));
    this->alt2->setText(QString::number(altPre, 'f', 1));
    this->alt->setText(QString::number(altGps, 'f', 1));
    this->lat->setText(QString::number(lat, 'f', 6));
    this->lng->setText(QString::number(lng, 'f', 6));
    this->pdop->setText(QString::number(pdop, 'f', 1));
}
