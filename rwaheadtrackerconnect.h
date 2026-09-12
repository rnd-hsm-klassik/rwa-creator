/*
 * This file is part of the Rwa Creator.
 * An open-source cross-platform Middleware for creating interactive Soundwalks
 *
 * Copyright (C) 2015 - 2022 Thomas Resch
 *
 * License: MIT
 *
 * rwaheadtrackerconnect.h
 * by Thomas Resch
 *
 * The headtracker link: BLE scan/connect (DeviceFinder/DeviceHandler), heading
 * calibration and step detection, and since rtk-rover >= 0.48.0 GPS positioning
 * and the NTRIP / RTCM corrections loop around it: the NTRIP client
 * (RwaNtripClient, alive while a connected assembly offers the RTCM downlink
 * and corrections are enabled), the GPS/RTK position feed for the hero, and the
 * live stats the Headtracker View shows.
 */

#ifndef RWAHEADTRACKERCONNECT_H
#define RWAHEADTRACKERCONNECT_H

#include <QObject>
#include <QRegularExpression>
#include <QDateTime>
#include <QElapsedTimer>
#include "bluetooth/devicefinder.h"
#include "bluetooth/devicehandler.h"
#include "rwacastersettings.h"
#include "rwantripclient.h"

/** Snapshot for the Headtracker View, taken at 1 Hz. */
struct RwaHeadtrackerStats
{
    // Bluetooth
    bool bleConnected = false;
    QString deviceName;
    QString targetName;
    // Heading (since the previous snapshot)
    double headingHz = 0;
    double intervalMeanMs = 0;
    double intervalSdMs = 0;
    double intervalMaxMs = 0;
    qint64 headingFrames = 0;      // cumulative since connect
    float azimuth = 0;
    float elevation = 0;
    // Corrections (NTRIP)
    bool correctionsEnabled = false;
    bool casterConfigured = false;
    QString caster;                // host:port/mount, no credentials
    bool rtcmDownlink = false;     // 713d0006 on the connected device
    QString ntripState;            // "", "connecting", "connected", "reconnecting", "disconnected"
    int ntripReconnects = 0;
    qint64 ntripBytesRx = 0;
    qint64 rtcmBytesWritten = 0;
    qint64 rtcmBytesDropped = 0;
    QString ntripError;            // last failure reason, cleared on connect
    QString lastGga;
    QDateTime lastGgaAt;
    bool ggaSeeded = false;        // the caster is (still) fed the hero position
    // Position (713d0004)
    bool heroFollowsRtk = false;
    bool hasPosition = false;
    double latitude = 0;
    double longitude = 0;
    QDateTime lastPositionAt;
    double positionHz = 0;
};

class RwaHeadtrackerConnect : public QObject
{
    Q_OBJECT
public:
    explicit RwaHeadtrackerConnect(QObject *parent = nullptr);
    static RwaHeadtrackerConnect *instance;
    static RwaHeadtrackerConnect *getInstance();
    float getAzimuth();
    QString getName();

    bool correctionsEnabled() const { return m_correctionsEnabled; }
    bool heroFollowsRtkPosition() const { return m_heroFollowsRtk; }

    /** Live numbers for the Headtracker View. Resets the heading interval
     *  window, so call it from one place at a steady rate. */
    RwaHeadtrackerStats stats();

public slots:
    void startBluetoothScanning();
    void disconnectHeadtracker();
    void setName(QString newName);
    void calibrateHeadtracker();

    /** Headtracker > NTRIP Corrections. Persisted. Starts or stops the
     *  caster session for an already connected assembly. */
    void setCorrectionsEnabled(bool enabled);
    /** Headtracker > Hero Follows RTK Position. Persisted. */
    void setHeroFollowsRtkPosition(bool enabled);
    /** Headtracker > NTRIP Caster... saved new settings: restart the
     *  session with them (the account is single-session, so the old one
     *  goes first). */
    void casterSettingsChanged();

private:
    DeviceHandler *m_handler;
    DeviceFinder *m_finder;
    QString name = "rwaht84";
    std::vector<float> headTrackerOrientation;
    std::vector<float> headTrackerOffset;

    uint32_t calibrationCounter = 0;
    uint32_t movingAverageIndex = 0;
    float movingAverageLinAccel[100];
    float averageAccel = 0;
    bool blockSteps = false;

    void collectCalibrationData(std::vector<float> &offsetVector, std::vector<float> &receivedOrientation, uint32_t &counter);
    void calculatedOrientation(std::vector<float> &receivedOrientation);
    void detectStep(float linAccelZ);
    void logStep(float linAccelZ);
    void noteHeadingFrame();
    void resetLinkStats();

    // Corrections (RTK GNSS)
    void startNtripClientIfPossible();
    void stopNtripClient();
    QString ggaSeedFromHero() const;

    bool m_correctionsEnabled = false;
    bool m_heroFollowsRtk = false;
    RwaNtripClient *m_ntrip = nullptr;
    RwaNtripClient::Status m_ntripStatus;
    QString m_ntripState;
    QString m_ntripError;
    QString m_lastGga;
    QDateTime m_lastGgaAt;
    bool m_ggaSeeded = false;

    // Heading rate / jitter window
    QElapsedTimer m_headingClock;
    bool m_headingClockValid = false;
    qint64 m_headingFrames = 0;
    int m_windowCount = 0;
    double m_windowSum = 0, m_windowSumSq = 0, m_windowMax = 0;
    QElapsedTimer m_windowClock;

    // Position
    bool m_hasPosition = false;
    double m_latitude = 0, m_longitude = 0;
    QDateTime m_lastPositionAt;
    int m_positionWindowCount = 0;

signals:
    void sendAzimuth(float azimuth);
    void sendElevation(float elevation);
    void sendZ(float Z);
    void sendStep();
    /** The RTK position (degrees), only while Hero Follows RTK Position is on. */
    void sendPosition(double latitude, double longitude);
    void correctionsEnabledChanged(bool enabled);
    void heroFollowsRtkPositionChanged(bool enabled);

private slots:
    void receiveHeadtrackerData(const QString &data);
    void receiveHeadtrackerSample(float azimuthDeg, float elevationDeg,
                                  float linAccelZ);
    void receivePosition(double latitude, double longitude);
    void receiveGga(const QString &sentence);
    void receiveRtcmDownlinkChanged(bool available);
    void receiveNtripStatus(const RwaNtripClient::Status &status);
    void receiveNtripError(const QString &reason);
    void unblockSteps();
};

#endif // RWAHEADTRACKERCONNECT_H
