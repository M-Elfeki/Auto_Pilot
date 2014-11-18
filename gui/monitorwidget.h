#pragma once
#include <QByteArray>
#include <QList>
#include <QMutex>
#include <QWidget>

class QCheckBox;
class QFile;
class QLabel;
class QSpinBox;
class QTextEdit;

/// GUI element to allow inspection of individual messages and history.
///
/// Stores all messages sent and received in a timestamped log file in the
/// working directory.
class MonitorWidget : public QWidget
{
    Q_OBJECT
public:
    /// Constructor.
    explicit MonitorWidget(QWidget *parent = 0);

public slots:
    /// Should be connected to a Vehicle::message.
    void onMessage(QByteArray message, bool incoming);

protected:
    /// Decrypt the encrypted portion of a message.
    ///
    /// If message is not recognized as an encrypted type the original message
    /// will be returned.
    /// @return message with any present encrypted portion decrypted.
    /// @param message Message to decrypt.
    static QByteArray decryptMessage(QByteArray message);

    /// Strip zigbee and config message headers
    /// @return message with any wrappers stripped.
    /// @param message Message to strip.
    static QByteArray stripMessage(QByteArray message);

    /// Convert a QByteArray into a hex QString.
    ///
    /// @return Hex bytes from message with separator character every spacing bytes.
    /// @param message Message to convert and pad.
    /// @param separator Character to use as separator.
    /// @param spacing Number of bytes that should be grouped together.
    static QString convertToHex(QByteArray message);

    /// Messages which are encrypted should be decrypted before displaying.
    QCheckBox *decrypt;

    /// Keep track of number of messages received.
    int incomingCount;

    /// Display incoming message count.
    QLabel *incomingCountL;

    /// Display selected incoming message.
    QTextEdit *incomingData;

    /// Display and allow changing of currently selected incoming message.
    QSpinBox *incomingIndex;

    /// The raw data of all incoming messages.
    QList<QByteArray> incomingMessages;

    /// Incoming message log.
    QFile *infile;

    /// Protect onMessage(...) in case threading is enabled.
    QMutex mutex;

    /// Outgoing message log.
    QFile *outfile;

    /// Keep track of number of messages sent.
    int outgoingCount;

    /// Display outgoing message count.
    QLabel *outgoingCountL;

    /// Display selected outgoing message.
    QTextEdit *outgoingData;

    /// Display and allow changing of currently selected outgoing message.
    QSpinBox *outgoingIndex;

    /// The raw data of all outgoing messages.
    QList<QByteArray> outgoingMessages;

    /// Strip wrappers from messages
    QCheckBox *stripWrappers;

protected slots:
    /// Decryption enabled or disabled. Reload currently displayed message.
    void decryptChanged(bool decrypt);

    /// Selected incoming message index changed.
    void incomingChanged(int index);

    /// Selected outgoing message index changed.
    void outgoingChanged(int index);
};
