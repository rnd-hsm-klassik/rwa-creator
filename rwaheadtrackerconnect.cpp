#include "rwaheadtrackerconnect.h"
#include "rwabackend.h"
#include "rwasimulator.h"
#include "rwaentity.h"
#include <QTimer>
#include <QSettings>
#include <QtMath>

// Set via target_compile_definitions in CMakeLists.txt
#ifndef RWA_VERSION
#define RWA_VERSION "unknown"
#endif

namespace {
const char *kCorrectionsEnabled = "headtracker/ntripcorrections";
const char *kHeroFollowsRtk = "headtracker/herofollowsrtkposition";
}

RwaHeadtrackerConnect *RwaHeadtrackerConnect::instance = nullptr;

RwaHeadtrackerConnect *RwaHeadtrackerConnect::getInstance()
{
    if(RwaHeadtrackerConnect::instance == nullptr)
    {
        RwaHeadtrackerConnect::instance = new RwaHeadtrackerConnect();
    }

    return RwaHeadtrackerConnect::instance;
}

float RwaHeadtrackerConnect::getAzimuth()
{
    return headTrackerOrientation[0];
}

RwaHeadtrackerConnect::RwaHeadtrackerConnect(QObject *parent) : QObject(parent)
{
    m_handler = new DeviceHandler(this);
    m_finder = new DeviceFinder(m_handler, this);

    // The handler dispatches per characteristic: decoded binary samples
    // from an RTK headtracker (713d0005), raw ASCII strings from an RWAHT
    // headtracker (713d0002). Both funnel into the same calibration /
    // step-detection path below.
    connect(m_handler, &DeviceHandler::headtrackerDataReceived,
            this, &RwaHeadtrackerConnect::receiveHeadtrackerData);
    connect(m_handler, &DeviceHandler::headtrackerSampleReceived,
            this, &RwaHeadtrackerConnect::receiveHeadtrackerSample);

    // RTK position feed and the corrections loop.
    connect(m_handler, &DeviceHandler::positionReceived,
            this, &RwaHeadtrackerConnect::receivePosition);
    connect(m_handler, &DeviceHandler::ggaReceived,
            this, &RwaHeadtrackerConnect::receiveGga);
    connect(m_handler, &DeviceHandler::rtcmDownlinkChanged,
            this, &RwaHeadtrackerConnect::receiveRtcmDownlinkChanged);
    connect(m_handler, &DeviceHandler::gnssFixReceived,
            this, &RwaHeadtrackerConnect::receiveGnssFix);
    connect(m_handler, &DeviceHandler::heartbeatReceived,
            this, &RwaHeadtrackerConnect::receiveHeartbeat);
    // The heading and position counters belong to the BLE link, whatever
    // firmware is on the other end.
    connect(m_handler, &DeviceHandler::aliveChanged, this, [this]() {
        if(!m_handler->alive())
            resetLinkStats();
    });

    headTrackerOrientation = std::vector<float>(3, 0.0);
    headTrackerOffset = std::vector<float>(3, 0.0);

    QSettings settings;
    m_correctionsEnabled = settings.value(kCorrectionsEnabled, false).toBool();
    m_heroFollowsRtk = settings.value(kHeroFollowsRtk, false).toBool();
}

void RwaHeadtrackerConnect::startBluetoothScanning()
{
    m_finder->setTargetName(name);
    m_finder->startSearch();
}

void RwaHeadtrackerConnect::disconnectHeadtracker()
{
    m_handler->disconnectService();
}

void RwaHeadtrackerConnect::setName(QString newName)
{
    name = newName;
}

QString RwaHeadtrackerConnect::getName()
{
    return name;
}

void RwaHeadtrackerConnect::calibrateHeadtracker()
{
    calibrationCounter = 1;
}

void RwaHeadtrackerConnect::collectCalibrationData(std::vector<float> &offsetVector, std::vector<float> &receivedOrientation, uint32_t &counter)
{
    if(counter <= 100)
    {
        offsetVector[0]+=receivedOrientation[0];
        offsetVector[1]+=receivedOrientation[1];
        offsetVector[2]+=receivedOrientation[2];
        counter++;
    }
    else
    {
        offsetVector[0]/=100;
        offsetVector[1]/=100;
        offsetVector[2]/=100;
        counter = 0;
        qDebug() << "Done Calibrating";
    }
}

void RwaHeadtrackerConnect::calculatedOrientation(std::vector<float> &receivedOrientation)
{
    headTrackerOrientation[0] = receivedOrientation[0] - headTrackerOffset[0];
    if(headTrackerOrientation[0] < 0)
        headTrackerOrientation[0]+=360;

    headTrackerOrientation[1] = receivedOrientation[1] - headTrackerOffset[1];
    headTrackerOrientation[2] = receivedOrientation[2] - headTrackerOffset[2];

    emit sendAzimuth(headTrackerOrientation[0]);
    emit sendElevation(headTrackerOrientation[1]);
    emit sendZ(headTrackerOrientation[2]);
}

void RwaHeadtrackerConnect::unblockSteps()
{
    blockSteps = false;
}

void RwaHeadtrackerConnect::detectStep(float linAccelZ)
{
    movingAverageLinAccel[movingAverageIndex] = linAccelZ;
    movingAverageIndex++;
    if(movingAverageIndex >= 100)
         movingAverageIndex = 0;

    averageAccel = 0;

    for(int i=0;i<100;i++)
        averageAccel += movingAverageLinAccel[i];

    averageAccel /= 100;

    if(!blockSteps)
    {
        if(linAccelZ >= 0)
        {
             if(averageAccel >= 0)
             {
                 float dif = linAccelZ-averageAccel;
                 if(dif > 0.6f)
                 {
                     logStep(linAccelZ);
                     emit sendStep();
                     QTimer::singleShot(500, this, SLOT(unblockSteps()));
                     blockSteps = true;
                 }
             }
             else
             {
                 float dif = linAccelZ+averageAccel;
                 if(dif > 0.6f)
                 {
                     logStep(linAccelZ);
                     emit sendStep();
                     QTimer::singleShot(500, this, SLOT(unblockSteps()));
                     blockSteps = true;
                 }
             }
         }
     }
 }

// Step events are derived here from linAccelZ (they are not wire data on
// either heading format), so this is their one log site - shared by the
// binary and ASCII paths.
void RwaHeadtrackerConnect::logStep(float linAccelZ)
{
    if(RwaBackend::getInstance()->logOther) {
        qInfo() << "Step detected (linAccelZ" << linAccelZ
                << ", moving avg" << averageAccel << ")";
    }
}

// Heading delivery rate and inter-arrival jitter
void RwaHeadtrackerConnect::noteHeadingFrame()
{
    m_headingFrames++;
    if(m_headingClockValid) {
        const double dt = m_headingClock.nsecsElapsed() / 1e6;
        m_windowCount++;
        m_windowSum += dt;
        m_windowSumSq += dt * dt;
        if(dt > m_windowMax)
            m_windowMax = dt;
    } else {
        m_headingClockValid = true;
        m_windowClock.start();
    }
    m_headingClock.start();
}

void RwaHeadtrackerConnect::receiveHeadtrackerSample(float azimuthDeg, float elevationDeg,
                                                     float linAccelZ)
{
    std::vector<float> receivedOrientation = {azimuthDeg, elevationDeg, 0.0f};
    noteHeadingFrame();

    // Payload parity with the ASCII path, which echoes the whole raw string:
    // the binary frame carries azimuth, elevation and linAccelZ.
    if(RwaBackend::getInstance()->logOther) {
        qInfo() << "BLE heading (azimuth/elevation):"
                << azimuthDeg << "/" << elevationDeg
                << "(binary, linAccelZ" << linAccelZ << ")";
    }

    detectStep(linAccelZ);

    if(calibrationCounter)
        collectCalibrationData(headTrackerOffset, receivedOrientation, calibrationCounter);
    else
        calculatedOrientation(receivedOrientation);
}

void RwaHeadtrackerConnect::receiveHeadtrackerData(const QString &data)
{
    std::vector<float> receivedOrientation = std::vector<float>(3, 0.0);

    if(data.isEmpty())
        return;

   // QStringList list = data.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
    QStringList list = data.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if(list.empty())
        return;

    noteHeadingFrame();

    if(list.length() >= 1)
        receivedOrientation[0] = list.at(0).toFloat();

    if(list.length() >= 2)
        receivedOrientation[1] = list.at(1).toFloat();

    if(RwaBackend::getInstance()->logOther && list.length() >= 2) {
        qInfo() << "BLE heading (azimuth/elevation):"
                << receivedOrientation[0]
                << "/" << receivedOrientation[1]
                << "(" << data << ")";
    }

    if(list.length() >= 3)
        detectStep(list.at(2).toFloat());

    if(calibrationCounter)
        collectCalibrationData(headTrackerOffset, receivedOrientation, calibrationCounter);
    else
        calculatedOrientation(receivedOrientation);
}

/** ******************************* RTK position (713d0004) ******************************* */

void RwaHeadtrackerConnect::receivePosition(double latitude, double longitude)
{
    m_hasPosition = true;
    m_latitude = latitude;
    m_longitude = longitude;
    m_lastPositionAt = QDateTime::currentDateTime();
    m_positionWindowCount++;

    if(RwaBackend::getInstance()->logCoordinates)
        qInfo() << "RTK position:" << QString::number(latitude, 'f', 7)
                << QString::number(longitude, 'f', 7);

    if(m_heroFollowsRtk)
        emit sendPosition(latitude, longitude);
}

void RwaHeadtrackerConnect::setHeroFollowsRtkPosition(bool enabled)
{
    if(m_heroFollowsRtk == enabled)
        return;
    m_heroFollowsRtk = enabled;
    QSettings settings;
    settings.setValue(kHeroFollowsRtk, enabled);
    qInfo() << "Hero follows RTK position:" << (enabled ? "on" : "off");
    emit heroFollowsRtkPositionChanged(enabled);
}

/** ******************************* Corrections loop ******************************* */

void RwaHeadtrackerConnect::setCorrectionsEnabled(bool enabled)
{
    if(m_correctionsEnabled == enabled)
        return;
    m_correctionsEnabled = enabled;
    QSettings settings;
    settings.setValue(kCorrectionsEnabled, enabled);
    qInfo() << "NTRIP corrections:" << (enabled ? "on" : "off");
    if(enabled)
        startNtripClientIfPossible();
    else
        stopNtripClient();
    emit correctionsEnabledChanged(enabled);
}

void RwaHeadtrackerConnect::casterSettingsChanged()
{
    // The caster account is single-session: the old session goes before
    // a new one, even if only the password changed.
    const bool wasRunning = m_ntrip != nullptr;
    stopNtripClient();
    if(wasRunning || m_handler->rtcmDownlinkAvailable())
        startNtripClientIfPossible();
}

void RwaHeadtrackerConnect::receiveRtcmDownlinkChanged(bool available)
{
    if(available) {
        startNtripClientIfPossible();
    } else {
        // The caster session ends with its only consumer.
        stopNtripClient();
        m_lastGga.clear();
        m_lastGgaAt = QDateTime();
    }
}

void RwaHeadtrackerConnect::resetLinkStats()
{
    m_headingClockValid = false;
    m_headingFrames = 0;
    m_windowCount = 0;
    m_windowSum = m_windowSumSq = m_windowMax = 0;
    m_positionWindowCount = 0;
    m_hasFix = false;
    m_hasHeartbeat = false;
}

/** ******************************* Telemetry (fix quality, health) ******************************* */

void RwaHeadtrackerConnect::receiveGnssFix(const RwaGnssFix &fix)
{
    m_hasFix = true;
    m_fix = fix;
    m_lastFixAt = QDateTime::currentDateTime();
    if(RwaBackend::getInstance()->logOther)
        qInfo() << "GNSS fix: type" << fix.fixType << "carrier" << fix.carrSoln
                << "hAcc" << fix.hAccMm << "mm vAcc" << fix.vAccMm << "mm sats" << fix.numSv
                << "corrAge" << (fix.corrAgeMs == 0xFFFFFFFF ? QStringLiteral("never") : QString::number(fix.corrAgeMs) + " ms");
}

void RwaHeadtrackerConnect::receiveHeartbeat(const RwaHeartbeat &heartbeat)
{
    m_hasHeartbeat = true;
    m_heartbeat = heartbeat;
    m_lastHeartbeatAt = QDateTime::currentDateTime();
    if(RwaBackend::getInstance()->logOther)
        qInfo() << "Assembly heartbeat: firmware" << heartbeat.fwVersion << "battery" << heartbeat.battMv
                << "mV, rtcm_bytes" << heartbeat.rtcmBytes << "free heap" << heartbeat.freeHeap;
}

/**
 * Opens the caster session once the RTCM downlink exists, corrections are
 * enabled and the settings are complete. A no-op while a session is already
 * running: there is never more than one client.
 */
void RwaHeadtrackerConnect::startNtripClientIfPossible()
{
    if(m_ntrip || !m_correctionsEnabled || !m_handler->rtcmDownlinkAvailable())
        return;
    const RwaCasterSettings settings = RwaCasterSettings::load();
    if(!settings.isComplete()) {
        qWarning() << "NTRIP: caster settings incomplete, no session (Headtracker > NTRIP Caster...)";
        m_ntripState.clear();
        m_ntripError = QStringLiteral("caster settings incomplete");
        return;
    }
    m_ntrip = new RwaNtripClient(settings, QStringLiteral(RWA_VERSION), this);
    m_ntrip->setGgaSeed([this]() { return ggaSeedFromHero(); });
    connect(m_ntrip, &RwaNtripClient::rtcmReceived,
            m_handler, &DeviceHandler::writeRtcm);
    connect(m_ntrip, &RwaNtripClient::statusChanged,
            this, &RwaHeadtrackerConnect::receiveNtripStatus);
    connect(m_ntrip, &RwaNtripClient::progress, this, [this](qint64 bytesRx) {
        m_ntripStatus.bytesRx = bytesRx;
    });
    connect(m_ntrip, &RwaNtripClient::errorOccurred,
            this, &RwaHeadtrackerConnect::receiveNtripError);
    m_ntripStatus = RwaNtripClient::Status();
    m_ntripState = QStringLiteral("connecting");
    m_ntripError.clear();
    // A GGA that arrived before the client existed is still the best
    // position for the VRS.
    if(!m_lastGga.isEmpty())
        m_ntrip->updateAssemblyGga(m_lastGga);
    m_ntrip->start();
}

void RwaHeadtrackerConnect::stopNtripClient()
{
    if(!m_ntrip)
        return;
    RwaNtripClient *client = m_ntrip;
    m_ntrip = nullptr;
    client->stop();
    client->deleteLater();
    m_ntripState.clear();
    m_ggaSeeded = false;
}

void RwaHeadtrackerConnect::receiveNtripStatus(const RwaNtripClient::Status &status)
{
    m_ntripStatus = status;
    m_ntripState = RwaNtripClient::stateName(status.state);
    if(status.state == RwaNtripClient::State::Connected)
        m_ntripError.clear();
}

void RwaHeadtrackerConnect::receiveNtripError(const QString &reason)
{
    m_ntripError = reason;
}

void RwaHeadtrackerConnect::receiveGga(const QString &sentence)
{
    m_lastGga = sentence;
    m_lastGgaAt = QDateTime::currentDateTime();
    m_ggaSeeded = false;
    if(m_ntrip)
        m_ntrip->updateAssemblyGga(sentence);
}

/**
 * The hero's map position as a GGA sentence: the VRS needs a position within a
 * few km, and the game is authored where it will be walked. Only until the
 * receiver delivers its own sentence. Empty without a game (no entity).
 */
QString RwaHeadtrackerConnect::ggaSeedFromHero() const
{
    if(RwaSimulator::entities.isEmpty())
        return QString();
    const std::vector<double> coords = RwaSimulator::entities.first()->getCoordinates();
    if(coords.size() < 2)
        return QString();
    const double lon = coords[0];
    const double lat = coords[1];
    if(lat == 0.0 && lon == 0.0)
        return QString();
    const_cast<RwaHeadtrackerConnect *>(this)->m_ggaSeeded = true;
    return RwaNtrip::gga(lat, lon, 0.0, QDateTime::currentDateTimeUtc());
}

/** ******************************* Stats snapshot ******************************* */

RwaHeadtrackerStats RwaHeadtrackerConnect::stats()
{
    RwaHeadtrackerStats s;
    s.bleConnected = m_handler->alive();
    s.deviceName = s.bleConnected ? m_handler->deviceName() : QString();
    s.targetName = name;

    if(m_windowCount > 0 && m_windowClock.isValid()) {
        const double seconds = m_windowClock.nsecsElapsed() / 1e9;
        const double mean = m_windowSum / m_windowCount;
        const double var = qMax(0.0, m_windowSumSq / m_windowCount - mean * mean);
        s.headingHz = seconds > 0 ? m_windowCount / seconds : 0;
        s.intervalMeanMs = mean;
        s.intervalSdMs = std::sqrt(var);
        s.intervalMaxMs = m_windowMax;
    }
    m_windowCount = 0;
    m_windowSum = m_windowSumSq = m_windowMax = 0;
    if(m_headingClockValid)
        m_windowClock.start();
    s.headingFrames = m_headingFrames;
    s.azimuth = headTrackerOrientation[0];
    s.elevation = headTrackerOrientation[1];

    const RwaCasterSettings caster = RwaCasterSettings::load();
    s.correctionsEnabled = m_correctionsEnabled;
    s.casterConfigured = caster.isComplete();
    s.caster = caster.isComplete() ? caster.endpointDescription() : QString();
    s.rtcmDownlink = m_handler->rtcmDownlinkAvailable();
    s.ntripState = m_ntripState;
    s.ntripReconnects = m_ntripStatus.reconnects;
    s.ntripBytesRx = m_ntripStatus.bytesRx;
    s.rtcmBytesWritten = m_handler->rtcmBytesWritten();
    s.rtcmBytesDropped = m_handler->rtcmBytesDropped();
    s.ntripError = m_ntripError;
    s.lastGga = m_lastGga;
    s.lastGgaAt = m_lastGgaAt;
    s.ggaSeeded = m_ggaSeeded && m_lastGga.isEmpty();

    s.hasFix = m_hasFix;
    s.fix = m_fix;
    s.lastFixAt = m_lastFixAt;
    s.hasHeartbeat = m_hasHeartbeat;
    s.heartbeat = m_heartbeat;
    s.lastHeartbeatAt = m_lastHeartbeatAt;

    s.heroFollowsRtk = m_heroFollowsRtk;
    s.hasPosition = m_hasPosition;
    s.latitude = m_latitude;
    s.longitude = m_longitude;
    s.lastPositionAt = m_lastPositionAt;
    s.positionHz = m_positionWindowCount;   // the snapshot cadence is 1 Hz
    m_positionWindowCount = 0;
    return s;
}
