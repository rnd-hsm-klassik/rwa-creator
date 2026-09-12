#include "rwaheadtrackerview.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDateTime>

namespace {

QString bytesText(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}

// Single-cell LiPo, ~4200 mV full to ~3300 mV empty:
// no discharge curve available, so this is a linear estimate.
QString batteryText(quint32 mv)
{
    if (mv == 0)
        return QStringLiteral("unknown");
    const int pct = qBound(0, int((double(mv) - 3300.0) / 900.0 * 100.0 + 0.5), 100);
    return QStringLiteral("%1 V (~%2 %)").arg(mv / 1000.0, 0, 'f', 2).arg(pct);
}

QString accuracyText(quint32 mm)
{
    if (mm < 1000)
        return QStringLiteral("%1 cm").arg(mm / 10.0, 0, 'f', 1);
    return QStringLiteral("%1 m").arg(mm / 1000.0, 0, 'f', 2);
}

QString fixTypeText(int v)
{
    switch (v) {
    case 0: return QStringLiteral("no fix");
    case 1: return QStringLiteral("dead reckoning");
    case 2: return QStringLiteral("2D");
    case 3: return QStringLiteral("3D");
    case 4: return QStringLiteral("GNSS + DR");
    default: return QString::number(v);
    }
}

QString carrSolnText(int v)
{
    switch (v) {
    case 0: return QStringLiteral("no RTK");
    case 1: return QStringLiteral("RTK float");
    case 2: return QStringLiteral("RTK fixed");
    default: return QString::number(v);
    }
}

QString ageText(const QDateTime &at)
{
    if (!at.isValid())
        return QString();
    const qint64 s = at.secsTo(QDateTime::currentDateTime());
    if (s < 60)
        return QStringLiteral("%1 s ago").arg(s);
    return QStringLiteral("%1 min ago").arg(s / 60);
}

} // namespace

RwaHeadtrackerView::RwaHeadtrackerView(QWidget *parent) : QWidget(parent)
{
    headtracker = RwaHeadtrackerConnect::getInstance();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto addGroup = [&](const QString &title) {
        QGroupBox *box = new QGroupBox(title, this);
        QFormLayout *form = new QFormLayout(box);
        form->setContentsMargins(8, 4, 8, 6);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(2);
        form->setLabelAlignment(Qt::AlignRight);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        layout->addWidget(box);
        return form;
    };

    QFormLayout *ble = addGroup(tr("Bluetooth"));
    bleState = addRow(ble, tr("Link"));
    headingRate = addRow(ble, tr("Heading rate"));
    headingJitter = addRow(ble, tr("Interval"));
    headingValues = addRow(ble, tr("Azimuth / elevation"));
    batteryLabel = addRow(ble, tr("Battery"));
    firmwareLabel = addRow(ble, tr("Firmware"));

    QFormLayout *ntrip = addGroup(tr("Corrections (NTRIP)"));
    correctionsState = addRow(ntrip, tr("Corrections"));
    casterLabel = addRow(ntrip, tr("Caster"));
    ntripState = addRow(ntrip, tr("Session"));
    rtcmBytes = addRow(ntrip, tr("RTCM"));
    ggaLabel = addRow(ntrip, tr("GGA to caster"));
    ntripError = addRow(ntrip, tr("Last error"));

    QFormLayout *pos = addGroup(tr("RTK position"));
    positionMode = addRow(pos, tr("Hero"));
    positionValue = addRow(pos, tr("Position"));
    fixLabel = addRow(pos, tr("Fix"));
    accuracyLabel = addRow(pos, tr("Accuracy"));

    layout->addStretch(1);

    timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &RwaHeadtrackerView::refresh);
    timer->start();
    refresh();
}

QLabel *RwaHeadtrackerView::addRow(QFormLayout *form, const QString &label)
{
    QLabel *value = new QLabel(this);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setWordWrap(true);
    form->addRow(label + ":", value);
    return value;
}

void RwaHeadtrackerView::setRow(QLabel *label, const QString &text, bool muted)
{
    label->setText(text);
    label->setEnabled(!muted);
}

void RwaHeadtrackerView::refresh()
{
    const RwaHeadtrackerStats s = headtracker->stats();

    // Bluetooth
    if (s.bleConnected)
        setRow(bleState, tr("connected to %1").arg(s.deviceName.isEmpty() ? s.targetName : s.deviceName));
    else
        setRow(bleState, tr("not connected (Headtracker > Connect via Bluetooth, name %1)").arg(s.targetName), true);
    if (s.bleConnected && s.headingHz > 0) {
        setRow(headingRate, QStringLiteral("%1 Hz (%2 frames)").arg(s.headingHz, 0, 'f', 1).arg(s.headingFrames));
        setRow(headingJitter, QStringLiteral("%1 ± %2 ms, max %3 ms")
               .arg(s.intervalMeanMs, 0, 'f', 1).arg(s.intervalSdMs, 0, 'f', 1).arg(s.intervalMaxMs, 0, 'f', 0));
        setRow(headingValues, QStringLiteral("%1° / %2°").arg(s.azimuth, 0, 'f', 1).arg(s.elevation, 0, 'f', 1));
    } else {
        setRow(headingRate, s.bleConnected ? tr("no frames") : QString(), true);
        setRow(headingJitter, QString(), true);
        setRow(headingValues, QString(), true);
    }

    if (s.bleConnected && s.hasHeartbeat) {
        QString battery = batteryText(s.heartbeat.battMv);
        if (s.lastHeartbeatAt.isValid() && s.lastHeartbeatAt.secsTo(QDateTime::currentDateTime()) > 45)
            battery += tr(" (stale, %1)").arg(ageText(s.lastHeartbeatAt));
        setRow(batteryLabel, battery, s.heartbeat.battMv == 0);
        setRow(firmwareLabel, s.heartbeat.fwVersion.isEmpty() ? tr("unknown") : s.heartbeat.fwVersion,
               s.heartbeat.fwVersion.isEmpty());
    } else {
        setRow(batteryLabel, s.bleConnected ? tr("waiting for heartbeat") : QString(), true);
        setRow(firmwareLabel, QString(), true);
    }

    // Corrections
    setRow(correctionsState, s.correctionsEnabled ? tr("enabled") : tr("disabled (Headtracker > NTRIP Corrections)"),
           !s.correctionsEnabled);
    setRow(casterLabel, s.casterConfigured ? s.caster : tr("not configured (Headtracker > NTRIP Caster...)"),
           !s.casterConfigured);
    if (!s.bleConnected)
        setRow(ntripState, QString(), true);
    else if (!s.rtcmDownlink)
        setRow(ntripState, tr("assembly has no RTCM downlink (firmware < 0.48.0)"), true);
    else if (s.ntripState.isEmpty())
        setRow(ntripState, tr("idle"), true);
    else
        setRow(ntripState, s.ntripReconnects > 0
               ? QStringLiteral("%1 (%2 reconnects)").arg(s.ntripState).arg(s.ntripReconnects)
               : s.ntripState);
    if (s.ntripBytesRx > 0 || s.rtcmBytesWritten > 0) {
        QString text = tr("%1 from caster, %2 to assembly").arg(bytesText(s.ntripBytesRx), bytesText(s.rtcmBytesWritten));
        if (s.rtcmBytesDropped > 0)
            text += tr(", %1 dropped").arg(bytesText(s.rtcmBytesDropped));
        setRow(rtcmBytes, text);
    } else {
        setRow(rtcmBytes, QString(), true);
    }
    if (!s.lastGga.isEmpty())
        setRow(ggaLabel, tr("receiver fix, %1").arg(ageText(s.lastGgaAt)));
    else if (s.ggaSeeded)
        setRow(ggaLabel, tr("hero position (receiver has no fix yet)"));
    else
        setRow(ggaLabel, s.bleConnected && s.rtcmDownlink ? tr("none yet") : QString(), true);
    setRow(ntripError, s.ntripError, s.ntripError.isEmpty());

    // Position
    setRow(positionMode, s.heroFollowsRtk ? tr("follows RTK position") : tr("map / OSC (Headtracker > Hero Follows RTK Position)"),
           !s.heroFollowsRtk);
    if (s.hasPosition) {
        QString text = QStringLiteral("%1, %2").arg(s.latitude, 0, 'f', 7).arg(s.longitude, 0, 'f', 7);
        if (s.positionHz > 0)
            text += QStringLiteral("  (%1 Hz)").arg(s.positionHz, 0, 'f', 0);
        else
            text += QStringLiteral("  (%1)").arg(ageText(s.lastPositionAt));
        setRow(positionValue, text, s.positionHz <= 0);
    } else {
        setRow(positionValue, s.bleConnected ? tr("no fix") : QString(), true);
    }
    if (s.bleConnected && s.hasFix) {
        // gnss_fix arrives at 1 Hz while the receiver delivers solutions;
        // a gap is the receiver-degraded signature, so say so.
        const bool stale = s.lastFixAt.isValid() && s.lastFixAt.secsTo(QDateTime::currentDateTime()) > 5;
        QString fix = QStringLiteral("%1, %2, %3 sats").arg(fixTypeText(s.fix.fixType), carrSolnText(s.fix.carrSoln)).arg(s.fix.numSv);
        fix += s.fix.corrAgeMs == 0xFFFFFFFF ? tr(", no corrections yet")
                                             : tr(", corrections %1 ms old").arg(s.fix.corrAgeMs);
        if (stale)
            fix += tr(" (stale, %1)").arg(ageText(s.lastFixAt));
        setRow(fixLabel, fix, stale || s.fix.fixType == 0);
        setRow(accuracyLabel, s.fix.fixType == 0 ? QString()
               : tr("horizontal %1, vertical %2").arg(accuracyText(s.fix.hAccMm), accuracyText(s.fix.vAccMm)),
               stale || s.fix.fixType == 0);
    } else {
        setRow(fixLabel, s.bleConnected ? tr("no telemetry (plain headtracker?)") : QString(), true);
        setRow(accuracyLabel, QString(), true);
    }
}
