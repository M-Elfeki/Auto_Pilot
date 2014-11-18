#pragma once

#include <QMap>
#include <QObject>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <SDL/SDL_joystick.h>

/// Simple wrapper for SDL joystick support.
class Joystick : public QObject
{
    Q_OBJECT
public:
    Joystick(QObject *parent = 0,
             int joystickEventTimeout = 20,
             bool doAutoRepeat = true,
             int repeatDelay = 250);
    ~Joystick();
    void close();
    QStringList getNames();
    int getNumAxes() { return numAxes; }
    int getNumBalls() { return numBalls; }
    int getNumButtons() { return numButtons; }
    int getNumHats() { return numHats; }
    bool isOpen() { return joystick != 0; }
    bool open(int index);

protected:
    bool autoRepeat;
    int autoRepeatDelay;
    QMap<int, int16_t> axes;
    QMap<int, QTime> axisRepeatTimers;
    QMap<int, QTime> buttonRepeatTimers;
    QMap<int, uint8_t> buttons;
    QMap<int, int> deadzones;
    int eventTimeout;
    QMap<int, QTime> hatRepeatTimers;
    QMap<int, uint8_t> hats;
    SDL_Joystick *joystick;
    QTimer *joystickTimer;
    QStringList names;
    int numAxes;
    int numButtons;
    int numHats;
    int numBalls;
    QMap<int, int> sensitivities;

signals:
    void axisValueChanged(int axis, int value);
    void buttonValueChanged(int button, bool value);
    void hatValueChanged(int hat, int value);
    void trackballValueChanged(int trackball, int deltaX, int deltaY);

protected slots:
    void processEvents();
};
