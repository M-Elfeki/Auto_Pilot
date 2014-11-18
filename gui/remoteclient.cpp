#include "remoteclient.h"
#include <QtEndian>
#include <QApplication>
#include <QBuffer>
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QTcpSocket>
#include <QTimer>
#include <QVBoxLayout>

static char const *format_string[] = { "PNG", "JPG", "PPM" };

enum Event {
    EventType,
    EventSize,
    EventKey,
    EventModifiers,
    EventText,
    EventAutorep,
    EventCount,
    EventPosition,
    EventButton,
    EventButtons
};

RemoteClient::RemoteClient(QWidget *parent) :
    QWidget(parent), alwaysVisible(false), buffer(), child(0), childMutex(), currentWidget(0), imageFormat(Format_JPG), lastWidget(0), serverMutex(), socket(new QTcpSocket(this))
{
    if (!qApp->arguments().contains("-v"))
        setAttribute(Qt::WA_DontShowOnScreen);
    else
        alwaysVisible = true;
    setUpdatesEnabled(true);
    setEnabled(true);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    connect(socket, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this,  SLOT(onSocketError(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

RemoteClient::ImageFormat RemoteClient::currentFormat() const
{
    return imageFormat;
}

bool RemoteClient::isConnected() const
{
    return (socket && socket->state() == QAbstractSocket::ConnectedState);
}

QWidget *RemoteClient::takeChild()
{
    return swapChild(0);
}

QWidget *RemoteClient::swapChild(QWidget *widget)
{
    QMutexLocker locker(&childMutex);
    if (child != widget) {
        std::swap(child, widget);
        lastWidget = currentWidget = 0;
        if (widget)
            widget->setParent(0);
        if (child)
            layout()->addWidget(child);
        QMetaObject::invokeMethod(child, "show", Qt::QueuedConnection);
        return widget;
    }
    return 0;
}

void RemoteClient::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QImage image(event->rect().size(), QImage::Format_RGB32);
    char const *format = format_string[imageFormat];
    QMutexLocker childLocker(&childMutex);
    if (child) {
        child->render(&image, QPoint(0, 0), event->rect());
        childLocker.unlock();
        QByteArray bytes;
        {
            QBuffer buffer(&bytes);
            if (!buffer.open(QIODevice::WriteOnly) ||
                    !image.save(&buffer, format)) {
                qCritical()<<"Failed to convert image";
                return;
            }
        }
        unsigned char header[11];
        header[0] = 0xDF;
        qToBigEndian<quint16>(bytes.length(), header + 1);
        qToBigEndian<quint16>(event->rect().x(), header + 3);
        qToBigEndian<quint16>(event->rect().y(), header + 5);
        sprintf((char *)header + 7, "%s", format);
        QMutexLocker socketLocker(&serverMutex);
        bytes.prepend((char *)header, 11);
        if (!socket || socket->write(bytes) < 0) {
            qCritical()<<"Failed to send image";
        }
    }
}

void RemoteClient::processEvent(QHash<int, QVariant> event)
{
    QMutexLocker locker(&childMutex);
    if (!child)
        return;
    QEvent::Type type = (QEvent::Type)event[EventType].toInt();
    if (type == QEvent::Resize) {
        curSize = event[EventSize].toSize();
        setFixedSize(curSize);
        update();
    } else if (type == QEvent::Show) {
        show();
        update();
    } else if (type == QEvent::Hide && !alwaysVisible) {
        hide();
    } else if (type == QEvent::KeyPress) {
        QKeyEvent *ev = new QKeyEvent(QKeyEvent::KeyPress, event[EventKey].toInt(), (Qt::KeyboardModifiers)event[EventModifiers].toInt(), event[EventText].toString(), event[EventAutorep].toBool(), event[EventCount].toInt());
        QApplication::postEvent(lastWidget, ev);
    } else if (type == QEvent::KeyRelease) {
        QKeyEvent *ev = new QKeyEvent(QKeyEvent::KeyRelease, event[EventKey].toInt(), (Qt::KeyboardModifiers)event[EventModifiers].toInt(), event[EventText].toString(), event[EventAutorep].toBool(), event[EventCount].toInt());
        QApplication::postEvent(lastWidget, ev);
    } else if (type == QEvent::MouseButtonDblClick) {
        QPoint pos = event[EventPosition].toPointF().toPoint();
        QWidget *recv = childAt(pos);
        if (recv == 0 || !isAncestorOf(recv))
            return;
        lastWidget = recv;
        QPoint relPos = recv->mapFrom(this, pos);
        QMouseEvent *ev = new QMouseEvent(QMouseEvent::MouseButtonDblClick, relPos, (Qt::MouseButton)event[EventButton].toInt(), (Qt::MouseButtons)event[EventButtons].toInt(), (Qt::KeyboardModifiers)event[EventModifiers].toInt());
        QApplication::postEvent(recv, ev);
    } else if (type == QEvent::MouseButtonPress) {
        QPoint pos = event[EventPosition].toPointF().toPoint();
        QWidget *recv = childAt(pos);
        if (recv == 0 || !isAncestorOf(recv))
            return;
        lastWidget = recv;
        QPoint relPos = recv->mapFrom(this, pos);
        QMouseEvent *ev = new QMouseEvent(QMouseEvent::MouseButtonPress, relPos, (Qt::MouseButton)event[EventButton].toInt(), (Qt::MouseButtons)event[EventButtons].toInt(), (Qt::KeyboardModifiers)event[EventModifiers].toInt());
        QApplication::postEvent(recv, ev);
        currentWidget = recv;
    } else if (type == QEvent::MouseButtonRelease) {
        QPoint pos = event[EventPosition].toPointF().toPoint();
        QWidget *recv = childAt(pos);
        if (recv == 0 || !isAncestorOf(recv))
            return;
        lastWidget = recv;
        QPoint relPos = recv->mapFrom(this, pos);
        QMouseEvent *ev = new QMouseEvent(QMouseEvent::MouseButtonRelease, relPos, (Qt::MouseButton)event[EventButton].toInt(), (Qt::MouseButtons)event[EventButtons].toInt(), (Qt::KeyboardModifiers)event[EventModifiers].toInt());
        currentWidget = 0;
        QApplication::postEvent(recv, ev);
    } else if (type == QEvent::MouseMove) {
        QPoint pos = event[EventPosition].toPointF().toPoint();
        if (currentWidget == 0 || !isAncestorOf(currentWidget))
            return;
        QPoint relPos = currentWidget->mapFrom(this, pos);
        QMouseEvent *ev = new QMouseEvent(QMouseEvent::MouseMove, relPos, (Qt::MouseButton)event[EventButton].toInt(), (Qt::MouseButtons)event[EventButtons].toInt(), (Qt::KeyboardModifiers)event[EventModifiers].toInt());
        QApplication::postEvent(currentWidget, ev);
    } else
        qWarning()<<"Unknown event received, type:"<<type;
}

void RemoteClient::onConnected()
{
    QTcpSocket *socket = dynamic_cast<QTcpSocket *>(sender());
    if (socket) {
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
        emit connected();
    }
}

void RemoteClient::onDisconnected()
{
    emit disconnected();
}

void RemoteClient::onSocketError(QAbstractSocket::SocketError error)
{
    qDebug()<<"socket error"<<error;
    socket->disconnectFromHost();
    emit disconnected();
}

void RemoteClient::onReadyRead()
{
    QHash<int,QVariant> event;
    buffer.append(socket->readAll());
    while (buffer.length() > 3) {
        if ((uchar)buffer.at(0) != 0xDF) {
            if (buffer.contains((char)0xDF)) {
                buffer = buffer.right(buffer.length() - buffer.indexOf((char)0xDF));
                qDebug()<<"trimming";
            } else {
                buffer.clear();
                qDebug()<<"clearing";
                return;
            }
        } else {
            quint16 len = qFromBigEndian<quint16>((uchar*)buffer.constData() + 1);
            if (len + 3 > buffer.length()) {
                qDebug()<<"too short";
                return;
            }
            buffer=buffer.right(buffer.length() - 3);
            {
                QBuffer buf(&buffer);
                buf.open(QIODevice::ReadOnly);
                QDataStream stream(&buf);
                stream>>event;
                buf.close();
            }
            if (event.isEmpty()) {
                buffer=buffer.right(buffer.length() - 1);
                continue;
            }
            buffer=buffer.right(buffer.length() - len);
            processEvent(event);
        }
    }
}

void RemoteClient::setChild(QWidget *widget)
{
    QWidget *old = swapChild(widget);
    if (old)
        old->deleteLater();
}

void RemoteClient::setFormat(ImageFormat format)
{
    if (format < static_cast<ImageFormat>(0) || format > nImageFormat) {
        format = Format_PNG;
        qWarning()<<"Invalid format specified, falling back to"<<format_string[format];
    }
    imageFormat = format;
}

void RemoteClient::connectToHost(QHostAddress serverAddress, quint16 serverPort)
{
    if (isConnected())
        disconnectFromHost();
    socket->connectToHost(serverAddress, serverPort);
}

void RemoteClient::disconnectFromHost()
{
    socket->disconnectFromHost();
}
