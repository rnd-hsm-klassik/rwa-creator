/***************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES IS DISCLAIMED.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "devicehandler.h"
#include "deviceinfo.h"

#include <QtMath>

// Custom BLE service exposed by the RWA headtracker (RFduino/Simblee style).
static const QBluetoothUuid rwaServiceUuid(
    QStringLiteral("{713d0000-503e-4c75-ba94-3148f18d941e}"));

// Heading characteristics under that service (PROJECT-PLAN.md §5.1/§5.5):
// binary frames from rtk-rover >= 0.46.0, ASCII text from the plain RWAHT headtracker.
// Everything else on the service (e.g. the RTK raw position on 713d0004) is not
// heading data and must not reach the text parser.
static const QBluetoothUuid binaryHeadingUuid(
    QStringLiteral("{713d0005-503e-4c75-ba94-3148f18d941e}"));
static const QBluetoothUuid asciiHeadingUuid(
    QStringLiteral("{713d0002-503e-4c75-ba94-3148f18d941e}"));
// RTK raw position (rtk-rover), ASCII "lat latHp lon lonHp".
static const QBluetoothUuid rawPositionUuid(
    QStringLiteral("{713d0004-503e-4c75-ba94-3148f18d941e}"));
// Corrections over BLE (rtk-rover >= 0.48.0, PROJECT-PLAN.md §5.6):
// RTCM downlink (write without response) and GGA uplink (notify).
static const QBluetoothUuid rtcmDownlinkUuid(
    QStringLiteral("{713d0006-503e-4c75-ba94-3148f18d941e}"));
static const QBluetoothUuid ggaUplinkUuid(
    QStringLiteral("{713d0007-503e-4c75-ba94-3148f18d941e}"));

namespace {

struct HeadingSample {
    float azimuth;
    float elevation;
    float linAccelZ;
};

// One binary heading frame (713d0005): 16 B little-endian,
// [seq u16][t_dev_ms u32][qi qj qk qw i16 Q14][linAccelZ i16 cm/s^2].
// seq/t_dev_ms are decoded nowhere here yet, the Creator has no
// site to show them yet.
//
// The angle math is the §5.5 canonical conversion, kept identical
// with rwa-player (Device.swift HeadingFrame). The atan2 forms are
// scale-invariant, so the quantized quaternion needs no normalization.
bool parseBinaryHeadingFrame(const QByteArray &value, HeadingSample &out)
{
    if (value.size() != 16)
        return false;

    const uchar *b = reinterpret_cast<const uchar *>(value.constData());
    auto i16 = [b](int o) {
        return qint16(quint16(b[o]) | quint16(b[o + 1]) << 8);
    };

    const float q14 = 16384.0f;
    const float qi = i16(6) / q14;
    const float qj = i16(8) / q14;
    const float qk = i16(10) / q14;
    const float qw = i16(12) / q14;

    float yaw = -std::atan2(2.0f * (qi * qj + qk * qw),
                            qi * qi - qj * qj - qk * qk + qw * qw)
                * 180.0f / float(M_PI);
    if (yaw < 0.0f)
        yaw += 360.0f;

    out.azimuth = yaw;
    out.elevation = -std::atan2(2.0f * (qj * qk + qi * qw),
                                -qi * qi - qj * qj + qk * qk + qw * qw)
                    * 180.0f / float(M_PI);
    out.linAccelZ = i16(14) / 100.0f;
    return true;
}

// The raw position frame (713d0004): "lat latHp lon lonHp", UBX
// high-precision integers, 1e-7 degrees plus a 1e-9-degree high-res part.
// Same decoding as rwa-player (Device.parseRawTrackerPosition). False on
// any malformed frame (never trap on radio data).
bool parseRawPosition(const QByteArray &value, double &latDeg, double &lonDeg)
{
    const QList<QByteArray> words = value.simplified().split(' ');
    if (words.size() != 4)
        return false;
    bool ok[4];
    const double lat   = words[0].toDouble(&ok[0]);
    const double latHp = words[1].toDouble(&ok[1]);
    const double lon   = words[2].toDouble(&ok[2]);
    const double lonHp = words[3].toDouble(&ok[3]);
    if (!ok[0] || !ok[1] || !ok[2] || !ok[3])
        return false;
    latDeg = lat * 1e-7 + latHp * 1e-9;
    lonDeg = lon * 1e-7 + lonHp * 1e-9;
    return true;
}

// One GGA notification (713d0007): the receiver's own $GPGGA / $GNGGA
// sentence, ASCII, no CRLF, one per notification (§5.6). Not parsed
// further: it is forwarded to the caster as is.
bool parseGgaSentence(const QByteArray &value, QString &sentence)
{
    if (value.contains('\r') || value.contains('\n'))
        return false;
    const QString text = QString::fromUtf8(value);
    if (!text.startsWith(QLatin1String("$GPGGA,")) && !text.startsWith(QLatin1String("$GNGGA,")))
        return false;
    sentence = text;
    return true;
}

} // namespace

DeviceHandler::DeviceHandler(QObject *parent) :
    BluetoothBaseClass(parent)
{
    // Writes without response are paced at one connection interval
    // (15 ms since rtk-rover 0.48.0), a few chunks per tick: far above
    // the ~1.4 KB/s a VRS delivers, even at the pre-MTU-exchange 20 B.
    m_rtcmPump = new QTimer(this);
    m_rtcmPump->setInterval(15);
    connect(m_rtcmPump, &QTimer::timeout, this, &DeviceHandler::pumpRtcm);
}

void DeviceHandler::setAddressType(AddressType type)
{
    switch (type) {
    case DeviceHandler::AddressType::PublicAddress:
        m_addressType = QLowEnergyController::PublicAddress;
        break;
    case DeviceHandler::AddressType::RandomAddress:
        m_addressType = QLowEnergyController::RandomAddress;
        break;
    }
}

DeviceHandler::AddressType DeviceHandler::addressType() const
{
    if (m_addressType == QLowEnergyController::RandomAddress)
        return DeviceHandler::AddressType::RandomAddress;

    return DeviceHandler::AddressType::PublicAddress;
}

void DeviceHandler::setDevice(DeviceInfo *device)
{
    clearMessages();
    m_currentDevice = device;

    // Disconnect and delete old connection
    resetCorrectionsLink();
    if (m_control) {
        m_control->disconnectFromDevice();
        delete m_control;
        m_control = nullptr;
    }

    // Create new controller and connect it if device available
    if (m_currentDevice) {
        m_control = QLowEnergyController::createCentral(m_currentDevice->getDevice(), this);
        m_control->setRemoteAddressType(m_addressType);
        connect(m_control, &QLowEnergyController::serviceDiscovered,
                this, &DeviceHandler::serviceDiscovered);
        connect(m_control, &QLowEnergyController::discoveryFinished,
                this, &DeviceHandler::serviceScanDone);

        connect(m_control, &QLowEnergyController::errorOccurred, this,
                [this](QLowEnergyController::Error error) {
                    Q_UNUSED(error);
                    setError("Cannot connect to remote device.");
                });
        connect(m_control, &QLowEnergyController::connected, this, [this]() {
            setInfo("Controller connected. Search services...");
            m_control->discoverServices();
        });
        connect(m_control, &QLowEnergyController::disconnected, this, [this]() {
            setError("LowEnergy controller disconnected");
            resetCorrectionsLink();
            emit aliveChanged();
        });

        // Connect
        m_control->connectToDevice();
    }
}

void DeviceHandler::serviceDiscovered(const QBluetoothUuid &gatt)
{
    if (gatt == rwaServiceUuid) {
        setInfo("RWA headtracker service discovered. Waiting for service scan to be done...");
        m_foundHeadtrackerService = true;
    }
}

void DeviceHandler::serviceScanDone()
{
    setInfo("Service scan done.");

    // Delete old service if available
    resetCorrectionsLink();
    if (m_service) {
        delete m_service;
        m_service = nullptr;
        m_notificationDescs.clear();  // descriptors died with the service
    }

    // If the headtracker service was found, create the service object
    if (m_foundHeadtrackerService)
        m_service = m_control->createServiceObject(rwaServiceUuid, this);

    if (m_service) {
        connect(m_service, &QLowEnergyService::stateChanged, this, &DeviceHandler::serviceStateChanged);
        connect(m_service, &QLowEnergyService::characteristicChanged, this, &DeviceHandler::handleCharacteristicData);
        connect(m_service, &QLowEnergyService::descriptorWritten, this, &DeviceHandler::confirmedDescriptorWrite);
        connect(m_service, &QLowEnergyService::errorOccurred, this, &DeviceHandler::serviceError);
        m_service->discoverDetails();
    } else {
        setError("RWA headtracker service not found.");
    }
}

void DeviceHandler::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    switch (s) {
    case QLowEnergyService::RemoteServiceDiscovering:
        setInfo(tr("Discovering services..."));
        break;
    case QLowEnergyService::RemoteServiceDiscovered:
    {
        setInfo(tr("Service discovered."));

        // subscribe by writing 0100 to the CCCD of every characteristic that has
        // one. deliberately do NOT filter on the Notify property flag - the
        // RWA headtracker firmware doesn't always report it, and the old working
        // code subscribed to every characteristic's CCCD too. Every subscribed
        // descriptor is tracked so disconnectService() can unsubscribe them all
        // (this used to keep only the last one found).
        m_notificationDescs.clear();
        const QList<QLowEnergyCharacteristic> chars = m_service->characteristics();
        qDebug() << "[BLE debug] RWA service characteristics:" << chars.size();
        for (const QLowEnergyCharacteristic &ch : chars) {
            qDebug() << "[BLE debug]   char" << ch.uuid().toString()
                     << "properties=" << int(ch.properties());
            if (ch.uuid() == rtcmDownlinkUuid) {
                // Write-only: no CCCD to subscribe, kept for writeRtcm().
                m_rtcmCharacteristic = ch;
                qDebug() << "[BLE debug]   -> RTCM downlink (rtk-rover >= 0.48.0), MTU"
                         << m_control->mtu();
                continue;
            }
            const QLowEnergyDescriptor cccd = ch.descriptor(
                QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
            if (cccd.isValid()) {
                m_notificationDescs.append(cccd);
                m_service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
                qDebug() << "[BLE debug]   -> subscribed (wrote 0100 to CCCD)";
            }
        }

        if (m_notificationDescs.isEmpty())
            setError("No characteristic with a CCCD found on RWA service.");

        if (m_rtcmCharacteristic.isValid())
            emit rtcmDownlinkChanged(true);

        break;
    }
    default:
        //nothing for now
        break;
    }

    emit aliveChanged();
}

void DeviceHandler::handleCharacteristicData(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    if (c.uuid() == binaryHeadingUuid) {
        HeadingSample sample;
        if (parseBinaryHeadingFrame(value, sample))
            emit headtrackerSampleReceived(sample.azimuth, sample.elevation,
                                           sample.linAccelZ);
        else
            qDebug() << "[BLE debug] malformed binary heading frame,"
                     << value.size() << "bytes";
        return;
    }

    if (c.uuid() == asciiHeadingUuid) {
        emit headtrackerDataReceived(QString::fromUtf8(value));
        return;
    }

    if (c.uuid() == rawPositionUuid) {
        double lat, lon;
        if (parseRawPosition(value, lat, lon))
            emit positionReceived(lat, lon);
        else
            qDebug() << "[BLE debug] malformed raw position frame:" << value;
        return;
    }

    if (c.uuid() == ggaUplinkUuid) {
        QString sentence;
        if (parseGgaSentence(value, sentence))
            emit ggaReceived(sentence);
        else
            qDebug() << "[BLE debug] malformed GGA notification:" << value;
        return;
    }

    // Other subscribed characteristics (future additions) carry no
    // heading and are ignored here.
}

void DeviceHandler::serviceError(QLowEnergyService::ServiceError error)
{
    if (error == QLowEnergyService::CharacteristicWriteError)
        qWarning() << "[BLE] characteristic write failed (RTCM downlink?)";
    else if (error != QLowEnergyService::NoError)
        qDebug() << "[BLE debug] service error" << int(error);
}

/** ******************************* RTCM writer ******************************* */

void DeviceHandler::writeRtcm(const QByteArray &data)
{
    if (!m_rtcmCharacteristic.isValid())
        return;
    m_rtcmQueue.append(data);
    const int excess = m_rtcmQueue.size() - rtcmQueueCapacity;
    if (excess > 0) {
        // Drop the *oldest* bytes, the policy of the firmware's own FIFO:
        // after a stall the newest corrections matter, not the backlog.
        m_rtcmQueue.remove(0, excess);
        m_rtcmDropped += excess;
    }
    if (!m_rtcmPump->isActive())
        m_rtcmPump->start();
    pumpRtcm();
}

void DeviceHandler::pumpRtcm()
{
    if (!m_service || !m_control || !m_rtcmCharacteristic.isValid()
            || m_service->state() != QLowEnergyService::RemoteServiceDiscovered) {
        m_rtcmPump->stop();
        return;
    }
    if (m_rtcmQueue.isEmpty()) {
        m_rtcmPump->stop();
        return;
    }
    // ATT MTU - 3 per write (514 at the 517 macOS negotiates); 20 until
    // the MTU exchange. Qt reports the negotiated value on the controller.
    const int maxLength = qMax(20, m_control->mtu() - 3);
    for (int i = 0; i < 4 && !m_rtcmQueue.isEmpty(); ++i) {
        const int n = qMin(maxLength, m_rtcmQueue.size());
        const QByteArray chunk = m_rtcmQueue.left(n);
        m_rtcmQueue.remove(0, n);
        m_service->writeCharacteristic(m_rtcmCharacteristic, chunk,
                                       QLowEnergyService::WriteWithoutResponse);
        m_rtcmWritten += n;
    }
}

/** Connect and disconnect: the downlink belongs to one connection and
 *  queued corrections are stale by the time a new one exists. */
void DeviceHandler::resetCorrectionsLink()
{
    const bool had = m_rtcmCharacteristic.isValid();
    m_rtcmPump->stop();
    m_rtcmQueue.clear();
    m_rtcmCharacteristic = QLowEnergyCharacteristic();
    if (had)
        emit rtcmDownlinkChanged(false);
}

void DeviceHandler::confirmedDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value)
{
    if (d.isValid() && m_notificationDescs.contains(d)
            && value == QByteArray::fromHex("0000")) {
        //disabled notifications -> assume disconnect intent
        m_notificationDescs.removeAll(d);
        if (m_notificationDescs.isEmpty()) {
            resetCorrectionsLink();
            m_control->disconnectFromDevice();
            delete m_service;
            m_service = nullptr;
            emit aliveChanged();
        }
    }
}

void DeviceHandler::disconnectService()
{
    m_foundHeadtrackerService = false;

    //disable notifications
    bool unsubscribing = false;
    if (m_service) {
        for (const QLowEnergyDescriptor &d : std::as_const(m_notificationDescs)) {
            if (d.isValid() && d.value() == QByteArray::fromHex("0100")) {
                m_service->writeDescriptor(d, QByteArray::fromHex("0000"));
                unsubscribing = true;
            }
        }
    }
    if (!unsubscribing) {
        resetCorrectionsLink();
        if (m_control)
            m_control->disconnectFromDevice();

        delete m_service;
        m_service = nullptr;
        m_notificationDescs.clear();
        emit aliveChanged();
    }
    // else: confirmedDescriptorWrite disconnects once every 0000 is confirmed.
}

bool DeviceHandler::alive() const
{
    if (m_service)
        return m_service->state() == QLowEnergyService::RemoteServiceDiscovered;

    return false;
}

QString DeviceHandler::deviceName() const
{
    return m_currentDevice ? m_currentDevice->getName() : QString();
}
