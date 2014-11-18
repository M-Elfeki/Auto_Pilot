#pragma once
#include <stdint.h>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class Joystick;

/// GUI element to allow setting of channel values or motor speeds.
class ControlWidget : public QWidget
{
    Q_OBJECT
public:
    /// Constructor.
    explicit ControlWidget(QWidget *parent = 0);

    /// Alter availability of arm/disarm buttons in response to a change in
    /// bypass-mode.
    void setBypass(bool bypass);

    /// Set joystick mode.
    /// @param enable If true allow joystick to be used.
    void setJoystick(bool enable);

    static int isDisarmed;
    static int throttle, yaw, roll, pitch;

signals:
    /// Emitted by ControlWidget::sendControls()
    /// @param c0 Roll, or motor #0
    /// @param c1 Pitch, or motor #1
    /// @param c2 Throttle, or motor #2
    /// @param c3 Yaw, or motor #3
    /// @param c4 Tilt, or motor #4
    /// @param c5 Ascent, or motor #5
    /// @param c6 Hold, or motor #6
    /// @param c7 Shutter, or motor #7
    void controls(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3,
                  uint8_t c4, uint8_t c5, uint8_t c6, uint8_t c7);

    /// Arm button was clicked.
    void armClicked();

    /// Disarm button was clicked.
    void disarmClicked();

protected:
    /// Send yaw and throttle to arm, useful for circle-limiter joysticks.
    QPushButton *armButton;

    /// List of all axes followed by all buttons provided by selected joystick.
    QStringList axesAndButtons;

    /// Channels or motor speeds.
    ///
    /// Regardless of connection mode these range [0, 100].
    /// Vehicle will scale and offset as appropriate for the connection mode.
    QSlider *cs[8];

    /// Send yaw and throttle to disarm, useful for circle-limiter joysticks.
    /// If in bypass mode channels are not altered, instead the signal is
    /// is forwarded to the Vehicle class and the appropriate command is sent.
    QPushButton *disarmButton;

    /// Joystick axis/button -> channel mapping.
    QComboBox *jc[8];

    /// Joystick axis/button inversion.
    QCheckBox *ji[8];

    /// Instance of SDL joystick wrapper, this is reused.
    Joystick *joystick;

    /// Names of all available joysticks.
    QComboBox *joysticks;

    /// Joystick button sticky
    QCheckBox *js[8];

    /// Joystick tri-state sticky button current direction.
    bool jsd[8];

    /// Total number of axes provided by joystick.
    int numAxes;

    /// Button to check for new joysticks
    QPushButton *refreshButton;

    /// Store bypass-mode to allow arm/disarm buttons to remain enabled through
    /// changes to joystick.
    bool useBypass;

    /// If true joystick input will be enabled.
    bool useJoystick;

protected slots:
    /// Invoked every time the selected joystick is changed.
    ///
    /// Updates the mapping drop-downs.
    /// @param index Index of newly selected joystick, -1 for no joystick.
    void joystickSelected(int index);

    /// Invoked every time a mapping is changed.
    ///
    /// If to an axis or button the GUI slider is disabled.
    /// If to "None" the GUI slider is enabled.
    /// @param index Index of axis/button entry to be taken for this channel's
    /// input.
    void mappingChanged(int index);

    /// Invoked every time an axis value is reported from the joystick.
    ///
    /// The corresponding slider is set and value will be sent in the next
    /// sendControls.
    /// @param axis Axis being reported.
    /// @param value Value of given axis.
    void onAxisChanged(int axis, int value);

    /// Invoked every time a button value is reported from the joystick.
    ///
    /// The corresponding slider is set and value will be sent in the next
    /// sendControls.
    /// @param button Button being reported.
    /// @param pressed True if given button is currently depressed.
    void onButtonChanged(int button, bool pressed);

    /// Clears existing joystick list and gets new list.
    void refreshJoysticks();

    /// Invoked by timer at 40Hz, used to update Vehicle's local channels or
    /// motors.
    void sendControls();

    void manualPilot();
    void autoPilot();
    void arm();
    void disarm();
    void land();
    void takeoff(int minThrottle);
    void stabilize(int seconds);
    void wait(int mSeconds);
    void delay(int mSeconds);
    void up(int time);
    void down(int time);
    void forward(int minpitch);
    void backward(int maxpitch);
    void left(int minRoll);
    void right(int maxRoll);


};
