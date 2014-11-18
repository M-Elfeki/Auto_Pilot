#pragma once
#include <QHostAddress>
#include <QWidget>
#include "com/vehicle.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class ControlWidget;
class MonitorWidget;
class TelemetryWidget;

/// GUI element to allow selection of serial port, initiate vehicle
/// enumeration, select target vehicle, configure connection and
/// control modes, initiate and terminate connection, and display
/// connection status.
///
/// Serves as the top-level window and contains the control, monitor and
/// telemetry widgets.<BR>
/// Owns the Vehicle interface.
class ConfigWidget : public QWidget
{
    Q_OBJECT
public:
    /// Constructor.
    explicit ConfigWidget(QHostAddress hostAddress = QHostAddress::Null, quint16 hostUdp = 0, QWidget *parent = 0);

signals:
    /// Emitted for changes to connection state.
    void connected(bool connected);

protected:
    /// Toggle connection.
    QPushButton *acquire;

    /// Determines whether to connect in config-only mode.
    QCheckBox *config;

    /// Enabled once CONNECTED and not at all in config-only mode.
    /// Except when joystick is enabled this is always enabled to allow
    /// selection and mapping joystick before connection.
    ControlWidget *controlWidget;

    /// Instruct Vehicle class to send command to enter bypass-mode.
    QPushButton *enterBypass;

    /// Address of network host when connecting through Dragan View.
    QHostAddress hostAddress;

    /// UDP port on host to use when connecting through Dragan View.
    quint16 hostUdp;

    /// Allow use of joystick, requires zigbee and not config-only.
    QCheckBox *joystick;

    /// Instruct Vehicle class to send command to leave bypass-mode.
    QPushButton *leaveBypass;

    /// Displays messages received from Vehicle::message(...).
    MonitorWidget *monitorWidget;

    /// Stores and displays a list of all available serial ports.
    QComboBox *portList;

    /// Refresh the list of available serial ports.
    QPushButton *scanPorts;

    /// Refresh the list of available vehicles.
    QPushButton *scanVehicles;

    /// Display the current Vehicle::VehicleState
    QLabel *status;

    /// If checked bit-packed telemetry will be streamed from the vehicle.
    QCheckBox *telemetry;

    /// Display bit-packed telemetry and IMU values.
    TelemetryWidget *telemetryWidget;

    /// Vehicle interface used to communicate with a vehicle and support
    /// enumeration.
    Vehicle *vehicle;

    /// Stores and displays a list of all discovered vehicles.
    QComboBox *vehicleList;

    /// Checkbox which when checked enables communication over a XBee module.
    QCheckBox *zigbee;

protected slots:
    /// Check state of other options to decide whether bypass-mode is being
    /// requested and change availablility of other options in response.
    void checkBypass();

    /// Invoked by the 'refresh ports' button. Sets the serial port drop-down
    /// list to those devices returned by QextSerialEnumerator::getPorts().
    void checkPorts();

    /// Invoked by the 'Connect' button.
    void connectClicked();

    /// Invoked by the 'refresh vehicles' button. Requests enumeration by
    /// Vehicle using the currently selected serial port.
    void enumerate();

    /// Invoked on change of joystick enabled.
    void joystickToggled(bool toggled);

    /// Invoked by Vehicle::vehicleFound for every response. If the vehicle
    /// doesn't exist already it is added.
    void onVehicleFound(uint64_t vehicleMac, uint8_t channel);

    /// Invoked by Vehicle::stateChanged() for every state change. Used to
    /// enable elements based on connection state.
    void onVehicleStateChanged(Vehicle::VehicleState state);
};
