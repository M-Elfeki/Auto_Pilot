#include "controlwidget.h"
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QTimer>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QMessageBox>
#include <QEventLoop>
#include "joystick/joystick.h"

int ControlWidget::isDisarmed = 0;
int ControlWidget::throttle = 50;
int ControlWidget::yaw = 50;
int ControlWidget::roll = 50;
int ControlWidget::pitch = 50;

ControlWidget::ControlWidget(QWidget *parent) :
    QWidget(parent),
    armButton(new QPushButton("Arm", this)),
    axesAndButtons(),
    cs(),
    disarmButton(new QPushButton("Disarm", this)),
    jc(),
    ji(),
    joystick(new Joystick(this)),
    joysticks(new QComboBox(this)),
    js(),
    jsd(),
    refreshButton(new QPushButton(
            style()->standardIcon(QStyle::SP_BrowserReload), "", this))
{
    QTimer *timer = new QTimer(this);
    QLabel *cv[8];
    QGridLayout *mainLayout = new QGridLayout(this);

    for (int i = 0; i < 8; i++) {
        mainLayout->addWidget(jc[i] = new QComboBox(this), 1, i);
        connect(jc[i], SIGNAL(currentIndexChanged(int)),
                this, SLOT(mappingChanged(int)));
        mainLayout->addWidget(ji[i] = new QCheckBox("Inv", this), 2, i);
        mainLayout->addWidget(js[i] = new QCheckBox("Sticky", this), 3, i);
        js[i]->setVisible(false);
        mainLayout->addWidget(cs[i] = new QSlider(this), 4, i);
        cs[i]->setMaximum(100);
        mainLayout->addWidget(cv[i] = new QLabel("0", this), 5, i);
        connect(cs[i], SIGNAL(valueChanged(int)), cv[i], SLOT(setNum(int)));
        mainLayout->addWidget(new QLabel("M#" + QString::number(i), this),
                              6, i);
    }

    mainLayout->addWidget(joysticks, 0, 0, 1, 2);
    mainLayout->addWidget(refreshButton, 0, 2, Qt::AlignLeft);
    mainLayout->addWidget(armButton, 0, 4, 1, 2);
    mainLayout->addWidget(disarmButton, 0, 6, 1, 2);
    mainLayout->addWidget(new QLabel("Roll", this), 7, 0);
    mainLayout->addWidget(new QLabel("Ptch", this), 7, 1);
    mainLayout->addWidget(new QLabel("Thrt", this), 7, 2);
    mainLayout->addWidget(new QLabel("Yaw", this), 7, 3);
    mainLayout->addWidget(new QLabel("Tilt", this), 7, 4);
    mainLayout->addWidget(new QLabel("Acnt", this), 7, 5);
    mainLayout->addWidget(new QLabel("Hold", this), 7, 6);
    mainLayout->addWidget(new QLabel("Shtr", this), 7, 7);

    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshJoysticks()));
    connect(joysticks, SIGNAL(currentIndexChanged(int)),
            this, SLOT(joystickSelected(int)));
    connect(timer, SIGNAL(timeout()), this, SLOT(sendControls()));
    connect(joystick, SIGNAL(axisValueChanged(int,int)),
            this, SLOT(onAxisChanged(int,int)));
    connect(joystick, SIGNAL(buttonValueChanged(int,bool)),
            this, SLOT(onButtonChanged(int,bool)));
    connect(armButton, SIGNAL(clicked()), this, SIGNAL(armClicked()));
    connect(disarmButton, SIGNAL(clicked()), this, SIGNAL(disarmClicked()));

    timer->start(25);
}

void ControlWidget::joystickSelected(int index)
{
    if (joystick->isOpen())
        joystick->close();
    joystick->open(index);
    axesAndButtons.clear();
    axesAndButtons.append("None");
    if (joystick->isOpen()) {
        numAxes = joystick->getNumAxes();
        for (int i = 0; i < numAxes; i++)
            axesAndButtons.append("Axis " + QString::number(i));
        for (int i = 0; i < joystick->getNumButtons(); i++)
            axesAndButtons.append("Button " + QString::number(i));
    }
    for (int i = 0; i < 8; i++) {
        jc[i]->clear();
        jc[i]->addItems(axesAndButtons);
    }
}

void ControlWidget::mappingChanged(int index)
{
    QComboBox *cb = (QComboBox*)sender();
    for (int i = 0; i < 8; i++) {
        if (jc[i] == cb) {
            cs[i]->setEnabled(index == 0);
            ji[i]->setEnabled(index > 0);
            js[i]->setVisible(index > numAxes);
        }
    }
}

void ControlWidget::onAxisChanged(int axis, int value)
{
    if (!useJoystick)
        return;
    int scaled = 100.0 * (value + 32768) / 65535.0;
    for (int i = 0; i < 8; i++) {
        if (jc[i]->currentIndex() - 1 == axis)
            cs[i]->setValue(ji[i]->isChecked()? 100 - scaled : scaled);
    }
}

void ControlWidget::onButtonChanged(int button, bool pressed)
{
    if (!useJoystick)
        return;
    int value = pressed? 100 : 0;
    for (int i = 0; i < 8; i++) {
        if (jc[i]->currentIndex() - (1 + numAxes) == button) {
            if (js[i]->isChecked()) {
                if (!pressed)
                    return;
                if (cs[i]->value() == 50) {
                    cs[i]->setValue(jsd[i]? 0 : 100);
                    jsd[i] = !jsd[i];
                } else {
                    cs[i]->setValue(50);
                }
            } else {
                cs[i]->setValue(ji[i]->isChecked()? 100 - value : value);
            }
        }
    }
}

void ControlWidget::refreshJoysticks()
{
    joysticks->clear();
    joysticks->addItems(joystick->getNames());
}

void ControlWidget::setBypass(bool bypass)
{
    useBypass = bypass;
    armButton->setVisible(bypass || useJoystick);
    disarmButton->setVisible(bypass || useJoystick);
}

void ControlWidget::setJoystick(bool enable)
{
    useJoystick = enable;
    refreshButton->setVisible(enable);
    armButton->setVisible(enable || useBypass);
    disarmButton->setVisible(enable || useBypass);
    joysticks->setVisible(enable);
    for (int i = 0; i < 8; i++) {
        ji[i]->setVisible(enable);
        jc[i]->setVisible(enable);
    }
}

void ControlWidget::sendControls()
{
    if(isDisarmed<2)
        autoPilot();
    //else
        //manualPilot();

}

void ControlWidget::manualPilot()
{
    int thrott = cs[2]->value();
    int yw = cs[3]->value();
    if (useJoystick && armButton->isDown()) {
        thrott = 0;
        yw = 100;
    } else if (useJoystick && disarmButton->isDown()) {
        thrott = 0;
        yw = 0;
    }
    emit controls(cs[0]->value(), cs[1]->value(), thrott,
                  yw, cs[4]->value(), cs[5]->value(),
                  cs[6]->value(), cs[7]->value());
    throttle = thrott;
    yaw = yw;
}

//Write a protocol for autopilot in autoPilot function :)
void ControlWidget::autoPilot()
{
    arm();

//    takeoff(10);//MinThrottle =10
    up(20);
    emit controls(roll, pitch, throttle, yaw, 0, 0, 100, 0);
    stabilize(3);//Stabilize for 2 seconds
    down(20);//Land Protocole
//    land();
    disarm();
}

void ControlWidget::takeoff(int minThrottle)
{
    if(isDisarmed==1)
    {
        minThrottle = 20;
        int tempThrottle = throttle;
        for(int i=tempThrottle;i>minThrottle;i--)
        {
            emit controls(roll, pitch, i, yaw, 0, 100, 0, 0);
            wait(13);
            throttle = i;
        }
        emit controls(roll, pitch, minThrottle, yaw, 0, 100, 0, 0);
        throttle = minThrottle;
        wait(100);
    }
}

void ControlWidget::land()
{
    if(isDisarmed==1)
    {
        for(int i=throttle;i<100;i++)
        {
            emit controls(roll, pitch, i, yaw, 0, 0, 50, 0);
            delay(13);
        }
        throttle = 100;
        delay(500);
    }
}

void ControlWidget::up(int time){
    if(isDisarmed==1)
    {
        for(int i=0;i<time;i++)
        {
            emit controls(roll, pitch, throttle, yaw, 0, 100, 50, 0);
            delay(13);
        }
        delay(500);
    }
}


void ControlWidget::down(int time)
{
    if(isDisarmed==1)
    {
        for(int i=0;i<time;i++)
        {
            emit controls(roll, pitch, throttle, yaw, 0, 0, 50, 0);
            delay(13);
        }
        delay(500);
    }
}

void ControlWidget::stabilize(int seconds)
{
    if(isDisarmed==1)
    {
        emit controls(roll, pitch, throttle, yaw, 0, 50, 50, 0);
        delay(50);
        for(int i =0;i<seconds*4;i++)
            wait(250);
    }
}

void ControlWidget::wait(int mSeconds)
{
    if(cs[2]->value()>0||cs[3]->value()>0)
    {
        isDisarmed=1;
        land();
        disarm();
        delay(3000);
        QMessageBox mb;
        mb.setText("Auto-Pilot Aborted");
        mb.exec();
        exit(0);
    }
    delay(mSeconds);
}

void ControlWidget::delay(int mSeconds)
{
    if(cs[0]->value()>0)
    {
        emit controls(0, 0, 0, 100, 0, 100, 0, 0);
        QTimer t_1;
        t_1.start(1000);
        QEventLoop loop_1;
        connect(&t_1,SIGNAL(timeout()), &loop_1, SLOT(quit()));
        loop_1.exec();
        emit controls(0, 0, 0, 0, 0, 0, 0, 0);
        QTimer t_2;
        t_2.start(1000);
        QEventLoop loop_2;
        connect(&t_2,SIGNAL(timeout()), &loop_2, SLOT(quit()));
        loop_2.exec();
        QMessageBox mb;
        mb.setText("Auto-Pilot Aborted");
        mb.exec();
        exit(0);
    }
    QTimer t;
    t.start(mSeconds);
    QEventLoop loop;
    connect(&t, SIGNAL(timeout()), &loop, SLOT(quit()));
    loop.exec();
}

void ControlWidget::forward(int minpitch)
{
    if(isDisarmed==1)
    {
        int temppitch = pitch;
        for(int i=temppitch;i>minpitch;i--)
        {
            emit controls(roll, i, throttle, yaw, 0, 100, 50, 0);
            wait(50);
            pitch = i;
        }
        emit controls(roll, minpitch, throttle, yaw, 0, 100, 50, 0);
        pitch = minpitch;
        wait(50);
    }
}

void ControlWidget::backward(int maxpitch)
{
    if(isDisarmed==1)
    {
        int temppitch = pitch;
        for(int i=temppitch;i<maxpitch;i++)
        {
            emit controls(roll, i, throttle, yaw, 0, 100, 50, 0);
            wait(50);
            pitch = i;
        }
        emit controls(roll, maxpitch, throttle, yaw, 0, 100, 50, 0);
        pitch = maxpitch;
        wait(50);
    }
}

void ControlWidget::right(int maxRoll)
{
    if(isDisarmed==1)
    {
        int tempRoll = roll;
        for(int i=tempRoll;i<maxRoll;i++)
        {
            emit controls(i, pitch, throttle, yaw, 0, 100, 50, 0);
            wait(50);
            roll = i;
        }
        emit controls(maxRoll, pitch, throttle, yaw, 0, 100, 50, 0);
        roll = maxRoll;
        wait(50);
    }
}

void ControlWidget::left(int minRoll)
{
    if(isDisarmed==1)
    {
        int tempRoll = roll;
        for(int i=tempRoll;i>minRoll;i--)
        {
            emit controls(i, pitch, throttle, yaw, 0, 100, 50, 0);
            wait(50);
            roll = i;
        }
        emit controls(minRoll, pitch, throttle, yaw, 0, 100, 50, 0);
        roll = minRoll;
        wait(50);
    }
}

void ControlWidget::arm()
{
    if(isDisarmed==1)
    {
        //Initialize controls
        emit controls(50, 50, 50, 50, 0, 0, 0, 0);
        //Arming Procedure
        emit controls(50, 50, 0, 75, 0, 0, 0, 0);
        delay(500);
        emit controls(50, 50, 0, 100, 0, 0, 0, 0);
        delay(5000);
        //Taking-off Trigger
        emit controls(50, 50, 50, 50, 0, 0, 0, 0);
        wait(500);
        wait(500);
        wait(500);
    }
}

void ControlWidget::disarm()
{
    if(isDisarmed==1)
    {
        //Disengagement Procedure
        emit controls(50, 50, 50, 50, 0, 100, 0, 0);
        delay(500);
        //Disarming Procedure
        emit controls(50, 50, 0, 100, 0, 0, 0, 0);
        delay(5000);
        //Terminating auto pilot Procedure
        emit controls(0, 0, 0, 0, 0, 0, 0, 0);
        delay(1000);
    }
    isDisarmed ++;
}
