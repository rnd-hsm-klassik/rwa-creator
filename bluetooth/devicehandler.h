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

// Adapted from the Qt "Heart Rate Game" example for the RWA headtracker:
// connects to the RWA BLE service (713d0000) and forwards heading data,
// dispatched per characteristic:
//   713d0005 (rtk-rover >= 0.46.0): 16 B binary frame, decoded here and
//            emitted as headtrackerSampleReceived(azimuth, elevation, accel)
//   713d0002 (RWAHT): legacy ASCII text, emitted raw via
//            headtrackerDataReceived()
//   713d0004 (rtk-rover): ASCII raw position "lat latHp lon lonHp",
//            emitted as positionReceived(lat, lon) in degrees
//   713d0007 (rtk-rover >= 0.48.0): the receiver's own GGA sentence,
//            emitted raw via ggaReceived() for the NTRIP client
//   anything else is ignored - it used to be mis-parsed as heading.
// For rtk-rover >= 0.48.0 the handler is also the RTCM writer: writeRtcm()
// feeds the caster's byte stream to 713d0006 (write without response) in
// MTU-sized chunks, drop-oldest beyond a few VRS epochs.

#ifndef DEVICEHANDLER_H
#define DEVICEHANDLER_H

#include "bluetoothbaseclass.h"

#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QTimer>

class DeviceInfo;

/** gnss_fix (the fields to be shown) */
struct RwaGnssFix {
    double latitude = 0, longitude = 0;
    float heightM = 0;
    int fixType = 0;       ///< UBX fixType: 0 none, 1 DR, 2 2D, 3 3D, 4 GNSS+DR
    int carrSoln = 0;      ///< 0 none, 1 RTK float, 2 RTK fixed
    quint32 hAccMm = 0, vAccMm = 0;
    int numSv = 0;
    float pdop = 0;
    quint32 corrAgeMs = 0xFFFFFFFF;   ///< 0xFFFFFFFF = never
};

/** heartbeat (the fields to be shown) */
struct RwaHeartbeat {
    quint32 uptimeMs = 0;
    quint32 freeHeap = 0;
    QString fwVersion;
    quint32 droppedFrames = 0;
    quint32 battMv = 0;    ///< 0 = unknown
    quint32 rtcmBytes = 0; ///< pushed into the receiver since the previous heartbeat
};

class DeviceHandler : public BluetoothBaseClass
{
    Q_OBJECT

    Q_PROPERTY(bool alive READ alive NOTIFY aliveChanged)
    Q_PROPERTY(AddressType addressType READ addressType WRITE setAddressType)

public:
    enum class AddressType {
        PublicAddress,
        RandomAddress
    };
    Q_ENUM(AddressType)

    DeviceHandler(QObject *parent = nullptr);

    void setDevice(DeviceInfo *device);
    void setAddressType(AddressType type);
    AddressType addressType() const;

    bool alive() const;
    QString deviceName() const;

    /** Feed caster bytes to the assembly. Order is the only framing
     *  (§5.6): FIFO, drop-oldest beyond rtcmQueueCapacity, written in
     *  chunks of at most MTU - 3 bytes as the link drains. */
    void writeRtcm(const QByteArray &data);
    bool rtcmDownlinkAvailable() const { return m_rtcmCharacteristic.isValid(); }
    qint64 rtcmBytesWritten() const { return m_rtcmWritten; }
    qint64 rtcmBytesDropped() const { return m_rtcmDropped; }

    static constexpr int rtcmQueueCapacity = 8192;   // a few VRS epochs, like the firmware FIFO

signals:
    void aliveChanged();
    // Emitted for every ASCII notification from an RWAHT headtracker
    // (713d0002). The payload is the raw string
    // ("azimuth elevation linAccelZ").
    void headtrackerDataReceived(const QString &data);
    // Emitted for every decoded binary heading frame from an RTK
    // headtracker (713d0005, rtk-rover >= 0.46.0). Degrees / m/s^2,
    // calibration offsets not yet applied.
    void headtrackerSampleReceived(float azimuthDeg, float elevationDeg,
                                   float linAccelZ);
    // The RTK raw position (713d0004), degrees, up to 10 Hz while the
    // receiver has a fix.
    void positionReceived(double latitudeDeg, double longitudeDeg);
    // One GGA sentence (713d0007, no CRLF) from the receiver, fix quality
    // >= 1 only, at most 1 Hz.
    void ggaReceived(const QString &sentence);
    // True once 713d0006 was found on the connected device (rtk-rover
    // >= 0.48.0), false when the link goes away.
    void rtcmDownlinkChanged(bool available);
    // Telemetry service events (rtk-rover only).
    void gnssFixReceived(const RwaGnssFix &fix);
    void heartbeatReceived(const RwaHeartbeat &heartbeat);

public slots:
    void disconnectService();

private:
    //QLowEnergyController
    void serviceDiscovered(const QBluetoothUuid &);
    void serviceScanDone();

    //QLowEnergyService
    void serviceStateChanged(QLowEnergyService::ServiceState s);
    void handleCharacteristicData(const QLowEnergyCharacteristic &c,
                                  const QByteArray &value);
    void confirmedDescriptorWrite(const QLowEnergyDescriptor &d,
                                  const QByteArray &value);
    void serviceError(QLowEnergyService::ServiceError error);
    void telemetryStateChanged(QLowEnergyService::ServiceState s);
    void handleTelemetryData(const QLowEnergyCharacteristic &c,
                             const QByteArray &value);
    void decodeTelemetryFrame(const QByteArray &payload);
    void requestStatusDump();
    void pumpRtcm();
    void resetCorrectionsLink();

    QLowEnergyController *m_control = nullptr;
    QLowEnergyService *m_service = nullptr;
    QLowEnergyService *m_telemetry = nullptr;
    QLowEnergyCharacteristic m_telemetryCtrl;
    QByteArray m_telemetryBuffer;
    bool m_foundTelemetryService = false;
    // Every CCCD subscribed on the RWA service; drained one confirmed
    // unsubscribe at a time in confirmedDescriptorWrite on disconnect.
    QList<QLowEnergyDescriptor> m_notificationDescs;
    // The subset above that belongs to the telemetry service (a descriptor
    // must be written through its own service object).
    QList<QLowEnergyDescriptor> m_telemetryDescs;
    DeviceInfo *m_currentDevice = nullptr;
    // Corrections downlink: the characteristic, the ordered byte
    // queue and the pacing timer (one connection interval).
    QLowEnergyCharacteristic m_rtcmCharacteristic;
    QByteArray m_rtcmQueue;
    QTimer *m_rtcmPump = nullptr;
    qint64 m_rtcmWritten = 0;
    qint64 m_rtcmDropped = 0;

    bool m_foundHeadtrackerService = false;
    QLowEnergyController::RemoteAddressType m_addressType = QLowEnergyController::PublicAddress;
};

#endif // DEVICEHANDLER_H
