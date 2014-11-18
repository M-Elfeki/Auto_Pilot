#include "monitorwidget.h"
#include <QtCore>
#include <QtEndian>
#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QTextEdit>
#include "com/vehicle.h"

MonitorWidget::MonitorWidget(QWidget *parent) :
    QWidget(parent),
    decrypt(new QCheckBox("Decrypt", this)),
    incomingCount(0),
    incomingCountL(new QLabel("/ 0", this)),
    incomingData(new QTextEdit(this)),
    incomingIndex(new QSpinBox(this)),
    incomingMessages(),
    infile(new QFile(this)),
    mutex(),
    outfile(new QFile(this)),
    outgoingCount(0),
    outgoingCountL(new QLabel("/ 0", this)),
    outgoingData(new QTextEdit(this)),
    outgoingIndex(new QSpinBox(this)),
    outgoingMessages(),
    stripWrappers(new QCheckBox("Strip wrappers", this))
{
    QDir logFolder(QDesktopServices::storageLocation(
                        QDesktopServices::DocumentsLocation));
    if (!logFolder.exists("logs"))
        logFolder.mkpath("logs");
    logFolder.cd("logs");
    QString timestamp = QString::number(
                QDateTime::currentDateTime().toTime_t());
    infile->setFileName(logFolder.absoluteFilePath(
                            "incoming_" + timestamp + ".log"));
    outfile->setFileName(logFolder.absoluteFilePath(
                             "outgoing_" + timestamp + ".log"));
    if (!infile->open(QFile::ReadWrite | QFile::Truncate)) {
        delete infile;
        infile = 0;
    }
    if (!outfile->open(QFile::ReadWrite | QFile::Truncate)) {
        delete outfile;
        outfile = 0;
    }

    incomingIndex->setRange(0, 0);
    outgoingIndex->setRange(0, 0);
    incomingCountL->setFixedWidth(100);
    outgoingCountL->setFixedWidth(100);
    incomingData->setReadOnly(true);
    outgoingData->setReadOnly(true);
    incomingData->setFontFamily("Courier");
    outgoingData->setFontFamily("Courier");

    QGridLayout *layout = new QGridLayout(this);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(4, 1);
    layout->addWidget(new QLabel("Incoming", this), 0, 0);
    layout->addWidget(incomingIndex, 0, 1);
    layout->addWidget(incomingCountL, 0, 2);
    layout->addWidget(incomingData, 1, 0, 1, 3);
    layout->addWidget(new QLabel("Outgoing", this), 0, 4);
    layout->addWidget(outgoingIndex, 0, 5);
    layout->addWidget(outgoingCountL, 0, 6);
    layout->addWidget(outgoingData, 1, 4, 1, 3);
    QHBoxLayout *options = new QHBoxLayout();
    layout->addLayout(options, 2, 0, 1, 7);
    options->addWidget(decrypt);
    options->addWidget(stripWrappers);

    connect(incomingIndex, SIGNAL(valueChanged(int)),
            this, SLOT(incomingChanged(int)));
    connect(outgoingIndex, SIGNAL(valueChanged(int)),
            this, SLOT(outgoingChanged(int)));
    connect(decrypt, SIGNAL(toggled(bool)),
            this, SLOT(decryptChanged(bool)));
    connect(stripWrappers, SIGNAL(toggled(bool)),
            this, SLOT(decryptChanged(bool)));
}

void MonitorWidget::decryptChanged(bool decrypt)
{
    (void)decrypt;
    incomingChanged(incomingIndex->value());
    outgoingChanged(outgoingIndex->value());
}

QByteArray MonitorWidget::decryptMessage(QByteArray message)
{
    int configStart = message.indexOf((char)0xFFU);
    if (configStart >= 0 && message.length() - configStart >= 6) {
        quint16 length = qFromBigEndian<quint16>(
                    (unsigned char const *)message.constData() +
                    configStart + 2);
        if (configStart + length + 6 <= message.length() &&
                message.at(configStart + 1) != 0x6) {
            Vehicle::decrypt((unsigned char const *)message.constData(),
                             (unsigned char *)message.data(),
                             teaKey, configStart + 4, length);
        }
    }
    return message;
}

void MonitorWidget::incomingChanged(int index)
{
    QByteArray message = incomingMessages.value(index - 1);
    if (decrypt->isChecked())
        message = decryptMessage(message);
    if (stripWrappers->isChecked())
        message = stripMessage(message);
    QString temp = convertToHex(message);
    if (incomingData->toPlainText() != temp) {
        incomingData->setPlainText(temp);
        incomingData->update();
    }
}

void MonitorWidget::onMessage(QByteArray message, bool incoming)
{
    QMutexLocker locker(&mutex);
    QDateTime current = QDateTime::currentDateTime();
    QString timestamp = QString::number(current.toTime_t() * 1000 +
            current.time().msec());
    timestamp.append(":");
    if (incoming) {
        if (infile)
            infile->write(timestamp.toAscii() +
                          message.toHex().toUpper() + '\n');
        incomingCount++;
        incomingIndex->setMaximum(incomingCount);
        incomingCountL->setText("/ " + QString::number(incomingCount));
        incomingMessages.append(message);
        if (incomingIndex->value() == incomingCount - 1)
            incomingIndex->setValue(incomingCount);
    } else {
        if (outfile)
            outfile->write(timestamp.toAscii() +
                           message.toHex().toUpper() + '\n');
        outgoingCount++;
        outgoingIndex->setMaximum(outgoingCount);
        outgoingCountL->setText("/ " + QString::number(outgoingCount));
        outgoingMessages.append(message);
        if (outgoingIndex->value() == outgoingCount - 1)
            outgoingIndex->setValue(outgoingCount);
    }
}

void MonitorWidget::outgoingChanged(int index)
{
    QByteArray message = outgoingMessages.value(index - 1);
    if (decrypt->isChecked())
        message = decryptMessage(message);
    if (stripWrappers->isChecked())
        message = stripMessage(message);
    QString temp = convertToHex(message);
    if (outgoingData->toPlainText() != temp) {
        outgoingData->setPlainText(temp);
        outgoingData->update();
    }
}

QByteArray MonitorWidget::stripMessage(QByteArray message)
{
    unsigned char const *data = (unsigned char const *)message.constData();
    int length = message.length();
    if (data[0] == 0x7EU) {
        data += 3;
        length -= 4;
    }
    if (data[0] == 0x0U || data[0] == 0x80U) {
        data += 11;
        length -= 11;
    }
    if (data[0] == 0xFFU) {
        data += 1;
        length -= 3;
    }
    return QByteArray((char const *)data, length);
}

QString MonitorWidget::convertToHex(QByteArray message)
{
    QByteArray hexBytes = message.toHex().toUpper();
    QByteArray paddedBytes(qCeil(1.5 * hexBytes.length()) - 1, ' ');
    char const *src = hexBytes.constData();
    char *dst = paddedBytes.data();
    for (int i = 0; i < hexBytes.length() / 2; ++i) {
        dst[3 * i] = src[2 * i];
        dst[3 * i + 1] = src[2 * i + 1];
    }
    return paddedBytes;
}
