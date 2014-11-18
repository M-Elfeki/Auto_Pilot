#pragma once
#include <QHostAddress>
#include <QMutex>
#include <QWidget>

class QTcpSocket;
/// Client-side Dragan View GUI interop.
///
/// This class holds a single arbitrary child widget and handles all necessary
/// communication with a running instance of Dragan View to achieve a
/// virtualized presence of the child widget within Dragan View. Once a TCP
/// connection is established Dragan View will open an additional 'tab', this
/// tab may be switched to in the same manner as the native tabs and
/// contains a close button which allows termination of the connection from the
/// host-side.
/// Client sends images for Dragan View to display on the client's content
/// surface. These images can (and where possible should) be limited to only
/// those regions which have been changed. This is supported by the horizontal
/// and vertical offsets which must preceed the image data. There are three
/// image formats supported, JPG should be used unless lossless imagery is
/// required in which case either PNG or PPM can be used depending on whether
/// network bandwidth or CPU is at premium.
/// Client is responsible for painting the entire surface it is granted from
/// DraganView, the size of which may change during execution. Content of
/// regions which have not been painted since the last resize or show event is
/// undefined.
/// Dragan View will redirect mouse, keyboard, resize, show and hide events from
/// the client's tab within Dragan View to the client over the TCP connection.
/// This class then applies those events to the child widget with input events
/// redirected to either the descendent of the child widget which is at the
/// offset within this class's local drawing region corresponding to the offset
/// of the mouse within the client's drawing region within Dragan View in the
/// case of mouse clicks, or in the case of keyboard input to the local focus
/// descendent of the child widget which is simply the local widget which last
/// received a click event. Show/hide and resize events are applied directly to this
/// class's instance (and automatically in turn to the child widget).
///
/// NOTE: CLOSING THE REMOTE TAB WITHIN DRAGANVIEW WILL CLOSE THE
/// TCP CONNECTION, REMOVE THE TAB AND ITS CONTENT PANE, AND FREE THOSE
/// RESOURCES. IT WILL *NOT* PREVENT CONTINUED TRANSMISSION OF UDP CONTROL
/// MESSAGES FROM THE CLIENT (This example application terminates when the TCP
/// connection is closed rendering the point moot).
class RemoteClient : public QWidget
{
    Q_OBJECT
public:
    /// The following image formats are supported.
    ///
    /// JPG is encouraged if lossy encoding is acceptable.
    enum ImageFormat {
        Format_PNG,
        Format_JPG,
        Format_PPM,
        nImageFormat
    };

    /// Constructor.
    ///
    /// Generally this will be a top-level widget; it may be a child, but if so
    /// should not have any resizing applied to it (layouts, etc.) as this will
    /// conflict with the size required and set by Dragan View.
    explicit RemoteClient(QWidget *parent = 0);

    /// @return ImageFormat that will be used for outgoing imagery.
    ImageFormat currentFormat() const;

    /// @return true if there is a live TCP connection.
    bool isConnected() const;

    /// Take the child widget, callee takes ownership, child is reparented to 0.
    /// @return current child widget or 0 if there is no child.
    QWidget *takeChild();

    /// Attempt to work well with parent layouts.
    QSize sizeHint() const { return curSize; }

    /// Attempt to work well with parent layouts.
    QSizePolicy sizePolicy() const {
        return QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); }

    /// Replace current child widget.
    ///
    /// If new child is 0 this is equivalent to takeChild. If new child equals
    /// old child 0 is returned and no changes are made.
    /// @param widget New child.
    /// @return Old child or 0 if there was no previous child.
    QWidget *swapChild(QWidget *widget);

signals:
    /// Connection has been established with Dragan View.
    void connected();

    /// Connection with Dragan View has been closed.
    void disconnected();

public slots:
    /// Set current child widget, deleting the old child if it exists.
    ///
    /// @param widget New child.
    void setChild(QWidget *widget);

    /// Set format to be used in image transmission.
    ///
    /// @param format ImageFormat.
    void setFormat(ImageFormat format);

    /// Attempt to open connection to an instance of Dragan View.
    ///
    /// @param serverAddress Address of machine running Dragan View.
    /// @param serverPort UDP port to use on server side.
    void connectToHost(QHostAddress serverAddress = QHostAddress::LocalHost, quint16 serverPort = 64444);

    /// Manually disconnect from server.
    void disconnectFromHost();

protected:
    /// Repaint local display if applicable then draw the updated regions of our
    /// child to the buffer, then send those regions as partial updates to
    /// Dragan View.
    ///
    /// @param event Event details (sp. we're interested in the regions).
    void paintEvent(QPaintEvent *event);

    /// Handle a GUI event sent to us from Dragan View.
    ///
    /// @param event Event.
    void processEvent(QHash<int, QVariant> event);

protected slots:
    /// Set socket options once connection is established.
    void onConnected();

    /// Terminate program on connection closed.
    void onDisconnected();

    /// Disconnect on socket error.
    ///
    /// @param error Error details.
    void onSocketError(QAbstractSocket::SocketError error);

    /// An event has been received.
    void onReadyRead();

private:
    /// If this is true then Hide events should be filtered.
    bool alwaysVisible;

    /// Buffer for TCP stream.
    QByteArray buffer;

    /// Child widget to virtualize.
    QWidget *child;

    /// Mutex must be held for all access of child.
    QMutex childMutex;

    /// Pointer to the descendent of child which has (remote) mouse-move focus.
    ///
    /// WARNING: This is null except while a mouse button is being held down
    /// inside the remote content area of Dragan View. THIS MAY BECOME NULL /
    /// INVALID WITHOUT WARNING. THIS POINTER MUST NOT BE DEREFERENCED! IT IS
    /// PROVIDED FOR USE IN THE EVENT SYSTEM ONLY!
    QWidget *currentWidget;

    /// Size of this widget, used to provide sizeHint.
    QSize curSize;

    /// Current image format.
    ImageFormat imageFormat;

    /// Pointer to the descendent of child which has (remote) keyboard focus.
    ///
    /// WARNING: THIS MAY BE INVALIDATED WITHOUT WARNING. DO NOT DEREFERENCE.
    /// USE ONLY WITH EVENT SYSTEM.
    QWidget *lastWidget;

    /// Mutex must be held for all access of the TCP socket.
    QMutex serverMutex;

    /// TCP connection to Dragan View.
    QTcpSocket *const socket;
};
