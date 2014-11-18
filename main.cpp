#include <stdlib.h>
#include <QApplication>
#include <QMetaType>
#include "gui/configwidget.h"
#include "gui/monitorwidget.h"
#include "gui/remoteclient.h"
#include "com/remotecontroller.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#ifdef Q_OS_LINUX
    setenv("SDL_JOYSTICK_DEVICE", "/dev/input/js0", 1);
#endif
    QWidget *w = 0;
    QStringList args = a.arguments();
    if (args.contains("-m")) {
        // Monitor-mode, passive.
        QHostAddress hostAddress = QHostAddress::LocalHost;
        uint16_t hostPort = 65213;
        QString hostString;
        int i = args.indexOf("-m");
        do {
            hostString = args.value(++i);
        } while (hostString.startsWith('-'));
        if (!hostString.isEmpty() && !hostString.startsWith('-')) {
            QHostAddress tempAddr(hostString.split(':').value(0));
            bool ok;
            uint16_t tempPort = hostString.split(':').value(1).toUInt(&ok);
            if (!tempAddr.isNull())
                hostAddress = tempAddr;
            if (ok)
                hostPort = tempPort;
        }
        w = new MonitorWidget();
        RemoteController *reader = new RemoteController(hostAddress, hostPort,
                                                        true, w);
        QObject::connect(reader, SIGNAL(message(QByteArray,bool)),
                         (MonitorWidget*)w, SLOT(onMessage(QByteArray,bool)));
        w->setWindowTitle("Draganflyer API Monitor (" +
                          hostAddress.toString() + ":" +
                          QString::number(hostPort) + ")");
    } else if (args.contains("-r")) {
        // Remote-mode, with DraganView GUI interop.
        QHostAddress hostAddress = QHostAddress::LocalHost;
        uint16_t hostPort = 64444;
        uint16_t hostUdp = 65213;
        QString hostString;
        int i = args.indexOf("-r");
        do {
            hostString = args.value(++i);
        } while (hostString.startsWith('-'));
        if (!hostString.isEmpty() && !hostString.startsWith('-')) {
            QHostAddress tempAddr(hostString.split(':').value(0));
            if (!tempAddr.isNull())
                hostAddress = tempAddr;
            bool ok;
            uint16_t tempPort = hostString.split(':').value(1).toUInt(&ok);
            if (ok)
                hostPort = tempPort;
            tempPort = hostString.split(':').value(2).toUInt(&ok);
            if (ok)
                hostUdp = tempPort;
        }
        RemoteClient *c = new RemoteClient();
        c->connect(c, SIGNAL(disconnected()), SLOT(close()));
        c->setChild(new ConfigWidget(hostAddress, hostUdp));
        w = c;
        w->setWindowTitle("Draganflyer API Example");
        qRegisterMetaType<QHostAddress>("QHostAddress");
        QMetaObject::invokeMethod(c, "connectToHost", Qt::QueuedConnection,
                                  Q_ARG(QHostAddress, hostAddress), Q_ARG(quint16, hostPort));
    } else if (args.contains("-x")) {
        // Remote-mode, GUI is only displayed locally.
        QHostAddress hostAddress = QHostAddress::LocalHost;
        uint16_t hostPort = 65213;
        QString hostString;
        int i = args.indexOf("-x");
        do {
            hostString = args.value(++i);
        } while (hostString.startsWith('-'));
        if (!hostString.isEmpty() && !hostString.startsWith('-')) {
            QHostAddress tempAddr(hostString.split(':').value(0));
            bool ok;
            uint16_t tempPort = hostString.split(':').value(1).toUInt(&ok);
            if (!tempAddr.isNull())
                hostAddress = tempAddr;
            if (ok)
                hostPort = tempPort;
        }
        w = new ConfigWidget(hostAddress, hostPort);
    } else {
        // Default local-only mode.
        w = new ConfigWidget();
        w->setWindowTitle("Draganflyer API Example");
    }
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setMinimumSize(640, 480);
    w->show();
    return a.exec();
}
