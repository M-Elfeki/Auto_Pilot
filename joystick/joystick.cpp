#include "joystick.h"
#include <SDL/SDL.h>

Joystick::Joystick(QObject *parent, int joystickEventTimeout,
                   bool doAutoRepeat, int repeatDelay)
    : QObject(parent),
      autoRepeat(doAutoRepeat),
      autoRepeatDelay(repeatDelay),
      axes(),
      axisRepeatTimers(),
      buttonRepeatTimers(),
      buttons(),
      deadzones(),
      eventTimeout(joystickEventTimeout),
      hatRepeatTimers(),
      hats(),
      joystick(0),
      joystickTimer(new QTimer(this)),
      names(),
      numAxes(0),
      numButtons(0),
      numHats(0),
      numBalls(0),
      sensitivities()
{
    connect(joystickTimer, SIGNAL(timeout()), this, SLOT(processEvents()));
}

Joystick::~Joystick()
{
    if (isOpen())
        close();
    else
        SDL_Quit();
}

void Joystick::close()
{
    joystickTimer->stop();
    if ( joystick )
        SDL_JoystickClose(joystick);
    joystick = 0;
    numAxes = numButtons = numHats = numBalls = 0;
    SDL_Quit();
}

QStringList Joystick::getNames()
{
    QStringList names;
    SDL_Quit();
    SDL_Init(SDL_INIT_JOYSTICK);
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        names.append(SDL_JoystickName(i));
    return names;
}

bool Joystick::open(int stick)
{
    if (isOpen())
        close();
    SDL_Init(SDL_INIT_JOYSTICK);
    joystick = SDL_JoystickOpen(stick);
    if (joystick) {
        numAxes = SDL_JoystickNumAxes(joystick);
        numButtons = SDL_JoystickNumButtons(joystick);
        numHats = SDL_JoystickNumHats(joystick);
        numBalls = SDL_JoystickNumBalls(joystick);
        joystickTimer->start(eventTimeout);
        return true;
    } else {
        SDL_Quit();
        return false;
    }
}

void Joystick::processEvents()
{
    if (!isOpen())
        return;
    SDL_JoystickUpdate();
    for (int i = 0; i < numAxes; i++) {
        int16_t moved = SDL_JoystickGetAxis(joystick, i);
        if (abs(moved) >= deadzones[i]) {
            if ((moved != axes[i])) {
                int deltaMoved = abs(axes[i] - moved);
                if (deltaMoved >= sensitivities[i])
                    emit axisValueChanged(i, moved);
                axes[i] = moved;
                axisRepeatTimers[i].restart();
            } else if (autoRepeat && moved != 0) {
                if (axisRepeatTimers[i].elapsed() >= autoRepeatDelay) {
                    emit axisValueChanged(i, moved);
                    axes[i] = moved;
                }
            } else
                axisRepeatTimers[i].restart();
        } else
            emit axisValueChanged(i, 0);
    }
    for (int i = 0; i < numButtons; i++) {
        uint8_t changed = SDL_JoystickGetButton(joystick, i);
        if (changed != buttons[i]) {
            emit buttonValueChanged(i, (bool)changed);
            buttons[i] = changed;
            buttonRepeatTimers[i].restart();
        } else if (autoRepeat && changed != 0) {
            if (buttonRepeatTimers[i].elapsed() >= autoRepeatDelay) {
                emit buttonValueChanged(i, (bool)changed);
                buttons[i] = changed;
            }
        } else
            buttonRepeatTimers[i].restart();
    }
    for (int i = 0; i < numHats; i++) {
        uint8_t changed = SDL_JoystickGetHat(joystick, i);
        if ((changed != hats[i])) {
            emit hatValueChanged(i, changed);
            hats[i] = changed;
            hatRepeatTimers[i].restart();
        } else if (autoRepeat && changed != 0) {
            if (hatRepeatTimers[i].elapsed() >= autoRepeatDelay) {
                emit hatValueChanged(i, changed);
                hats[i] = changed;
            }
        } else
            hatRepeatTimers[i].restart();
    }
    for (int i = 0; i < numBalls; i++) {
        int dx, dy;
        SDL_JoystickGetBall(joystick, i, &dx, &dy);
        if (dx != 0 || dy != 0)
            emit trackballValueChanged(i, dx, dy);
    }
}
