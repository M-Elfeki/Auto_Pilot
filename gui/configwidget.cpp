#include "configwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "com/serial/qextserialenumerator.h"
#include "controlwidget.h"
#include "monitorwidget.h"
#include "telemetrywidget.h"

ConfigWidget::ConfigWidget(QHostAddress hostAddress, quint16 hostUdp, QWidget *parent) :
    QWidget(parent),
    acquire(new QPushButton("Connect", this)),
    config(new QCheckBox("Config", this)),
    controlWidget(new ControlWidget(this)),
    enterBypass(new QPushButton("Bypass-On", this)),
    hostAddress(hostAddress),
    hostUdp(hostUdp),
    joystick(new QCheckBox("Joystick", this)),
    leaveBypass(new QPushButton("Bypass-Off", this)),
    monitorWidget(new MonitorWidget(this)),
    portList(new QComboBox(this)),
    scanPorts(new QPushButton(
            style()->standardIcon(QStyle::SP_BrowserReload), "", this)),
    scanVehicles(new QPushButton(
            style()->standardIcon(QStyle::SP_BrowserReload), "", this)),
    status(new QLabel("IDLE", this)),
    telemetry(new QCheckBox("Telemetry", this)),
    telemetryWidget(new TelemetryWidget(this)),
    vehicle(new Vehicle(this)),
    vehicleList(new QComboBox(this)),
    zigbee(new QCheckBox("Zigbee", this))

{
    controlWidget->setEnabled(false);
    zigbee->setChecked(true);
    config->setChecked(false);
    vehicleList->setEnabled(true);
    scanVehicles->setEnabled(true);
    controlWidget->setJoystick(false);
    enterBypass->setEnabled(false);
    leaveBypass->setEnabled(false);

    connect(acquire, SIGNAL(clicked()),
            this, SLOT(connectClicked()));
    connect(controlWidget,
            SIGNAL(controls(uint8_t,uint8_t,uint8_t,uint8_t,
                            uint8_t,uint8_t,uint8_t,uint8_t)),
            vehicle,
            SLOT(setControls(uint8_t,uint8_t,uint8_t,uint8_t,
                             uint8_t,uint8_t,uint8_t,uint8_t)));
    connect(scanPorts, SIGNAL(clicked()),
            this, SLOT(checkPorts()));
    connect(scanVehicles, SIGNAL(clicked()),
            this, SLOT(enumerate()));
    connect(telemetry, SIGNAL(toggled(bool)),
            vehicle, SLOT(streamTelemetry(bool)));
    connect(vehicle, SIGNAL(message(QByteArray,bool)),
            monitorWidget, SLOT(onMessage(QByteArray,bool)));
    connect(vehicle,
            SIGNAL(telemetry1Changed(float,float,float,int,int,uint,float,int,
                                     int,int,float,float,float,float,float,
                                     float,float,uint,int,int,int,float)),
            telemetryWidget,
            SLOT(telemetry1(float,float,float,int,int,uint,float,int,int,int,
                            float,float,float,float,float,float,float,uint,int,
                            int,int,float)));
    connect(vehicle,
            SIGNAL(telemetry2Changed(float,float,float,int,int,uint,float,int,
                                     double,double,float,float,float,int,float,
                                     uint)),
            telemetryWidget,
            SLOT(telemetry2(float,float,float,int,int,uint,float,int,double,
                            double,float,float,float,int,float,uint)));
    connect(vehicle, SIGNAL(imuChanged(int16_t,int16_t,int16_t,
                                       int16_t,int16_t,int16_t)),
            telemetryWidget, SLOT(bypassImu(int16_t,int16_t,int16_t,
                                            int16_t,int16_t,int16_t)));
    connect(vehicle, SIGNAL(vehicleFound(uint64_t,uint8_t)),
            this, SLOT(onVehicleFound(uint64_t,uint8_t)));
    connect(vehicle, SIGNAL(stateChanged(Vehicle::VehicleState)),
            this, SLOT(onVehicleStateChanged(Vehicle::VehicleState)));
    connect(joystick, SIGNAL(toggled(bool)),
            this, SLOT(joystickToggled(bool)));
    connect(zigbee, SIGNAL(toggled(bool)),
            scanVehicles, SLOT(setEnabled(bool)));
    connect(zigbee, SIGNAL(toggled(bool)),
            vehicleList, SLOT(setEnabled(bool)));
    connect(zigbee, SIGNAL(toggled(bool)),
            this, SLOT(checkBypass()));
    connect(config, SIGNAL(toggled(bool)),
            this, SLOT(checkBypass()));
    connect(enterBypass, SIGNAL(clicked()),
            vehicle, SLOT(enterBypass()));
    connect(leaveBypass, SIGNAL(clicked()),
            vehicle, SLOT(leaveBypass()));
    connect(this, SIGNAL(connected(bool)),
            enterBypass, SLOT(setEnabled(bool)));
    connect(this, SIGNAL(connected(bool)),
            leaveBypass, SLOT(setEnabled(bool)));
    connect(controlWidget, SIGNAL(armClicked()),
            vehicle, SLOT(armHeli()));
    connect(controlWidget, SIGNAL(disarmClicked()),
            vehicle, SLOT(disarmHeli()));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *topLayout = new QHBoxLayout();
    QGridLayout *configLayout = new QGridLayout();
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(controlWidget, 2);
    mainLayout->addWidget(monitorWidget, 1);
    topLayout->addLayout(configLayout);
    topLayout->addWidget(telemetryWidget, 1);
    configLayout->addWidget(portList, 0, 0);
    configLayout->addWidget(scanPorts, 0, 1, Qt::AlignLeft);
    configLayout->addWidget(zigbee, 0, 2);
    configLayout->addWidget(vehicleList, 1, 0);
    configLayout->addWidget(scanVehicles, 1, 1, Qt::AlignLeft);
    configLayout->addWidget(config, 1, 2);
    configLayout->addWidget(acquire, 2, 0);
    configLayout->addWidget(status, 2, 1, 1, 2);
    configLayout->addWidget(joystick, 3, 0);
    configLayout->addWidget(telemetry, 3, 1, 1, 2);
    configLayout->addWidget(enterBypass, 4, 0);
    configLayout->addWidget(leaveBypass, 4, 1, 1, 2);

    if (!hostAddress.isNull() && hostUdp > 0) {
        portList->setVisible(false);
        scanPorts->setVisible(false);
        config->setChecked(true);
        config->setVisible(false);
        zigbee->setChecked(false);
        zigbee->setVisible(false);
        vehicleList->setVisible(false);
        scanVehicles->setVisible(false);
    } else
        checkPorts();
    checkBypass();
}

void ConfigWidget::checkPorts()
{
    portList->clear();
    // Add all serial ports to the list. In most cases the user will want to
    // use a CP2102 or similar so select the first one if found.
    bool foundUart = false;
    foreach (QextPortInfo info, QextSerialEnumerator::getPorts()) {
#ifdef Q_OS_WIN
        portList->addItem(info.portName);
#else
        portList->addItem(info.physName);
#endif
        if (!foundUart && info.physName.contains("USB")) {
            portList->setCurrentIndex(portList->count() - 1);
            foundUart = true;
        }
    }
}

void ConfigWidget::connectClicked()
{
    qDebug()<<hostAddress<<hostUdp;
    switch (vehicle->getState()) {
    case Vehicle::IDLE:
        // If idle we can try to connect.
        if (zigbee->isChecked())
            vehicle->open(portList->currentText(),
                          vehicleList->currentText().toULongLong(0, 16),
                          vehicleList->itemData(
                              vehicleList->currentIndex()).toUInt(),
                          config->isChecked());
        else if (!hostAddress.isNull() && hostUdp > 0)
            vehicle->open(hostAddress, hostUdp);
        else
            vehicle->open(portList->currentText(), config->isChecked());
        break;
    case Vehicle::CONNECTED:
    case Vehicle::CONNECTING:
        // Otherwise we should disconnect / cancel connection attempt.
        vehicle->close();
        break;
    default:
        break;
    }
    controlWidget->isDisarmed = 0;
}

void ConfigWidget::enumerate()
{
    // Clear entries between enumerations, even if the Vehicles available are
    // the same their channels may be different.
    vehicleList->clear();
    vehicle->enumerate(portList->currentText());
}

void ConfigWidget::joystickToggled(bool toggled)
{
    if (toggled) {
        //zigbee->setChecked(true);
        //config->setChecked(!zigbee->isChecked());
    }
    //zigbee->setEnabled(!toggled);
    //config->setEnabled(!toggled);
    controlWidget->setEnabled(toggled);
    controlWidget->setJoystick(toggled);
}

void ConfigWidget::onVehicleFound(uint64_t vehicleMac, uint8_t channel)
{
    // Add an item to the list of vehicles with the MAC address as a hex string
    // for the display text and the ZigBee channel for data, unless that
    // vehicle already exists.
    QString mac = QString::number(vehicleMac, 16).toUpper();
    for (int i = 0; i < vehicleList->count(); i++)
        if (vehicleList->itemText(i) == mac)
            return;
    vehicleList->addItem(mac, channel);
}

void ConfigWidget::onVehicleStateChanged(Vehicle::VehicleState state)
{
    // Config-only and ZigBee mode cannot be altered if connecting/connected.
    config->setEnabled(!joystick->isChecked() &&
                       (state == Vehicle::IDLE || state == Vehicle::ENUM));
    zigbee->setEnabled(!joystick->isChecked() &&
                       (state == Vehicle::IDLE || state == Vehicle::ENUM));
    joystick->setEnabled(state == Vehicle::IDLE);

    // Ports and vehicles can only be selected and enumerated while idle.
    portList->setEnabled(state == Vehicle::IDLE);
    vehicleList->setEnabled(state == Vehicle::IDLE && zigbee->isChecked());
    scanPorts->setEnabled(state == Vehicle::IDLE);
    scanVehicles->setEnabled(state == Vehicle::IDLE && zigbee->isChecked());

    // Cannot (dis-)connect while enumerating.
    acquire->setEnabled(state != Vehicle::ENUM);
    // Controls are enabled only if we are connected without config-only.
    controlWidget->setEnabled(joystick->isChecked() ||
                              (state == Vehicle::CONNECTED &&
                               (!config->isChecked() || !zigbee->isChecked())));

    if (state == Vehicle::IDLE) {
        acquire->setText("Connect");
        status->setText("IDLE");
    } else if (state == Vehicle::ENUM) {
        acquire->setText("Connect");
        status->setText("ENUM");
    } else if (state == Vehicle::CONNECTING) {
        acquire->setText("Cancel");
        status->setText("CONNECTING");
    } else {
        acquire->setText("Disconnect");
        status->setText("CONNECTED");
    }
    emit connected(state == Vehicle::CONNECTED);
}

void ConfigWidget::checkBypass()
{
    bool bypassMode = !zigbee->isChecked() && !config->isChecked();
    if (bypassMode)
        joystick->setChecked(false);
    joystick->setEnabled(!bypassMode);
    controlWidget->setBypass(bypassMode);
    enterBypass->setVisible(bypassMode);
    leaveBypass->setVisible(bypassMode);
}
