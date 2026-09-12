#include "rwantripclient.h"

#include <QTcpSocket>
#include <QTimer>
#include <QDebug>
#include <QtMath>

/** ************************************ Protocol pieces (pure) ************************************ */

namespace RwaNtrip {

QByteArray request(const RwaCasterSettings &settings, const QString &appVersion)
{
    const QByteArray credentials = (settings.user + ':' + settings.pass).toUtf8().toBase64();
    QByteArray text;
    text += "GET /" + settings.mount.toUtf8() + " HTTP/1.0\r\n";
    text += "User-Agent: NTRIP RWA Creator/" + appVersion.toUtf8() + "\r\n";
    text += "Authorization: Basic " + credentials + "\r\n";
    text += "\r\n";
    return text;
}

static const QByteArray crlf("\r\n");
static const QByteArray blankLine("\r\n\r\n");

bool parseResponse(const QByteArray &buffer, ParsedResponse &out)
{
    const int lineEnd = buffer.indexOf(crlf);
    if (lineEnd < 0)
        return false;
    const QString statusLine = QString::fromUtf8(buffer.left(lineEnd));
    if (statusLine.startsWith(QLatin1String("HTTP/")) || statusLine.startsWith(QLatin1String("SOURCETABLE "))) {
        const int blank = buffer.indexOf(blankLine);
        if (blank < 0)
            return false;
        const QString header = QString::fromUtf8(buffer.left(blank));
        out.response = classify(statusLine, header);
        out.statusLine = statusLine;
        out.headerLength = blank + blankLine.size();
        return true;
    }
    int end = lineEnd + crlf.size();
    if (buffer.size() - end >= 2 && buffer.mid(end, 2) == crlf)
        end += 2;
    out.response = classify(statusLine, statusLine);
    out.statusLine = statusLine;
    out.headerLength = end;
    return true;
}

Response classify(const QString &statusLine, const QString &header)
{
    const QStringList words = statusLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.size() < 2)
        return Response::Other;
    bool ok = false;
    const int code = words[1].toInt(&ok);
    if (!ok)
        return Response::Other;
    if (words[0] == QLatin1String("SOURCETABLE"))
        return Response::SourceTable;
    if (code == 401)
        return Response::Unauthorized;
    if (code == 200 && (words[0] == QLatin1String("ICY") || words[0].startsWith(QLatin1String("HTTP/")))) {
        // NTRIP 2.0 casters answer an unknown mount point with a plain
        // HTTP 200 whose body is the source table.
        if (header.toLower().contains(QLatin1String("gnss/sourcetable")))
            return Response::SourceTable;
        return Response::StreamOpen;
    }
    return Response::Other;
}

QString checksum(const QString &body)
{
    quint8 c = 0;
    for (const char byte : body.toUtf8())
        c ^= quint8(byte);
    return QString::asprintf("%02X", c);
}

bool hasValidChecksum(const QString &sentence)
{
    if (!sentence.startsWith(QLatin1Char('$')))
        return false;
    const int star = sentence.lastIndexOf(QLatin1Char('*'));
    if (star < 0)
        return false;
    const QString body = sentence.mid(1, star - 1);
    const QString given = sentence.mid(star + 1);
    return given.toUpper() == checksum(body);
}

QString degreesMinutes(double degrees, int degreeDigits)
{
    const qint64 unitsPerMinute = 100000;
    const qint64 unitsPerDegree = 60 * unitsPerMinute;
    const qint64 units = qint64(qRound64(degrees * double(unitsPerDegree)));
    const qint64 whole = units / unitsPerDegree;
    const qint64 minuteUnits = units % unitsPerDegree;
    return QStringLiteral("%1%2.%3")
        .arg(whole, degreeDigits, 10, QLatin1Char('0'))
        .arg(minuteUnits / unitsPerMinute, 2, 10, QLatin1Char('0'))
        .arg(minuteUnits % unitsPerMinute, 5, 10, QLatin1Char('0'));
}

QString gga(double latitude, double longitude, double altitude, const QDateTime &utc)
{
    const QDateTime t = utc.toUTC();
    const QString time = t.toString(QStringLiteral("HHmmss")) + '.'
        + QStringLiteral("%1").arg(t.time().msec() / 10, 2, 10, QLatin1Char('0'));
    const QString body = QStringLiteral("GPGGA,%1,%2,%3,%4,%5,1,08,1.0,%6,M,0.0,M,,")
        .arg(time)
        .arg(degreesMinutes(qAbs(latitude), 2), latitude >= 0 ? QStringLiteral("N") : QStringLiteral("S"))
        .arg(degreesMinutes(qAbs(longitude), 3), longitude >= 0 ? QStringLiteral("E") : QStringLiteral("W"))
        .arg(QString::number(altitude, 'f', 1));
    return '$' + body + '*' + checksum(body);
}

} // namespace RwaNtrip

/** ***************************************** Client ***************************************** */

QString RwaNtripClient::stateName(State s)
{
    switch (s) {
    case State::Disconnected: return QStringLiteral("disconnected");
    case State::Connected:    return QStringLiteral("connected");
    case State::Reconnecting: return QStringLiteral("reconnecting");
    }
    return QString();
}

RwaNtripClient::RwaNtripClient(const RwaCasterSettings &settings, const QString &appVersion,
                               QObject *parent)
    : QObject(parent), m_settings(settings), m_appVersion(appVersion)
{
    qRegisterMetaType<RwaNtripClient::Status>("RwaNtripClient::Status");

    m_deadline = new QTimer(this);
    m_deadline->setSingleShot(true);
    connect(m_deadline, &QTimer::timeout, this, &RwaNtripClient::deadlineFired);

    m_tick = new QTimer(this);
    m_tick->setInterval(1000);
    connect(m_tick, &QTimer::timeout, this, &RwaNtripClient::tickFired);

    m_attempt = new QTimer(this);
    m_attempt->setSingleShot(true);
    connect(m_attempt, &QTimer::timeout, this, &RwaNtripClient::connectToCaster);
}

RwaNtripClient::~RwaNtripClient()
{
    // No status report from a destructor: the owner already dropped us.
    m_wasConnected = false;
    closeSession();
}

void RwaNtripClient::start()
{
    if (m_running)
        return;
    m_running = true;
    qInfo() << "NTRIP: start, caster" << m_settings.endpointDescription();
    openSession();
}

void RwaNtripClient::stop()
{
    if (!m_running)
        return;
    m_running = false;
    m_attempt->stop();
    closeSession();
    qInfo() << "NTRIP: stopped";
}

void RwaNtripClient::updateAssemblyGga(const QString &sentence)
{
    m_assemblyGga = sentence;
}

/** ***************************************** Session ***************************************** */

void RwaNtripClient::openSession()
{
    if (!m_running)
        return;
    if (m_successfulConnects > 0 && !m_reconnectingReported) {
        m_reconnectingReported = true;
        report(State::Reconnecting);
    }
    if (m_attemptDelayMs > 0) {
        const int delay = m_attemptDelayMs;
        m_attemptDelayMs = 0;
        qInfo() << "NTRIP: backoff, next attempt in" << delay / 1000 << "s";
        m_attempt->start(delay);
        return;
    }
    connectToCaster();
}

void RwaNtripClient::connectToCaster()
{
    if (!m_running || m_socket)
        return;
    if (m_settings.port == 0) {
        fail(QStringLiteral("invalid caster port"), true);
        return;
    }
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &RwaNtripClient::socketConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &RwaNtripClient::socketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &RwaNtripClient::socketError);
    connect(m_socket, &QTcpSocket::disconnected, this, &RwaNtripClient::socketDisconnected);
    m_phase = Phase::Connecting;
    m_responseBuffer.clear();
    m_gotDataThisSession = false;
    armDeadline(connectTimeoutMs, QStringLiteral("TCP connect to caster timed out"));
    qInfo() << "NTRIP: connecting to" << m_settings.host << ":" << m_settings.port;
    m_socket->connectToHost(m_settings.host, m_settings.port);
}

void RwaNtripClient::socketConnected()
{
    sendRequest();
}

void RwaNtripClient::sendRequest()
{
    m_phase = Phase::AwaitingResponse;
    qInfo() << "NTRIP: requesting /" + m_settings.mount;
    m_socket->write(RwaNtrip::request(m_settings, m_appVersion));
    armDeadline(responseTimeoutMs, QStringLiteral("caster response timed out"));
}

void RwaNtripClient::socketReadyRead()
{
    if (!m_socket)
        return;
    const QByteArray data = m_socket->readAll();
    if (!data.isEmpty())
        received(data);
}

void RwaNtripClient::socketError()
{
    if (!m_socket)
        return;
    const QString reason = m_socket->errorString();
    if (m_phase == Phase::Connecting)
        fail(QStringLiteral("connection failed: %1").arg(reason), true);
    else
        fail(QStringLiteral("receive failed: %1").arg(reason), !m_gotDataThisSession);
}

void RwaNtripClient::socketDisconnected()
{
    if (!m_socket)
        return;
    fail(QStringLiteral("caster closed the connection"), !m_gotDataThisSession);
}

void RwaNtripClient::received(const QByteArray &data)
{
    switch (m_phase) {
    case Phase::AwaitingResponse: {
        m_responseBuffer.append(data);
        RwaNtrip::ParsedResponse parsed;
        if (!RwaNtrip::parseResponse(m_responseBuffer, parsed)) {
            if (m_responseBuffer.size() > maxHeaderLength)
                fail(QStringLiteral("caster response header too large"), true);
            return;
        }
        const QByteArray rest = m_responseBuffer.mid(parsed.headerLength);
        m_responseBuffer.clear();
        switch (parsed.response) {
        case RwaNtrip::Response::StreamOpen:
            streamOpened();
            if (!rest.isEmpty())
                streamBytes(rest);
            break;
        case RwaNtrip::Response::SourceTable:
            fail(QStringLiteral("mount point /%1 unknown to the caster (source table)").arg(m_settings.mount), true);
            break;
        case RwaNtrip::Response::Unauthorized:
            fail(QStringLiteral("caster refused the credentials (401)"), true);
            break;
        case RwaNtrip::Response::Other:
            fail(QStringLiteral("unexpected caster response: %1").arg(parsed.statusLine), true);
            break;
        }
        break;
    }
    case Phase::Streaming:
        streamBytes(data);
        break;
    case Phase::Idle:
    case Phase::Connecting:
        break;
    }
}

void RwaNtripClient::streamOpened()
{
    cancelDeadline();
    m_phase = Phase::Streaming;
    m_stripLeadingBlankLine = true;
    m_lastData.start();
    m_successfulConnects++;
    m_reconnectingReported = false;
    m_wasConnected = true;
    qInfo() << "NTRIP: stream open (connect #" << m_successfulConnects << ")";
    report(State::Connected);
    sendGga(true);
    m_tick->start();
}

void RwaNtripClient::streamBytes(const QByteArray &data)
{
    QByteArray payload = data;
    if (m_stripLeadingBlankLine) {
        m_stripLeadingBlankLine = false;
        if (payload.size() >= 2 && payload.left(2) == RwaNtrip::crlf)
            payload.remove(0, 2);
    }
    if (payload.isEmpty())
        return;
    m_lastData.start();
    if (!m_gotDataThisSession) {
        m_gotDataThisSession = true;
        m_reconnectDelayMs = backoffStartMs;   // received data resets the backoff
    }
    m_bytesRx += payload.size();
    emit rtcmReceived(payload);
}

/** Once per second while streaming: the no-RTCM hangup and the GGA push. */
void RwaNtripClient::tickFired()
{
    if (m_phase != Phase::Streaming)
        return;
    const int timeout = m_gotDataThisSession ? rtcmTimeoutMs : connectGraceMs;
    if (m_lastData.elapsed() > timeout) {
        // A session that never delivered a byte counts as a failed
        // attempt: back off before hammering the caster again.
        fail(QStringLiteral("no RTCM for %1 s, dropping the caster connection").arg(timeout / 1000),
             !m_gotDataThisSession);
        return;
    }
    if (m_bytesRx != m_bytesRxReported) {
        m_bytesRxReported = m_bytesRx;
        emit progress(m_bytesRx);
    }
    sendGga(false);
}

/** The latest GGA + CRLF, right after the 200 and then every 10 s. Rev2
 *  VRS casters stream nothing until they have one. Without any sentence
 *  (no assembly fix yet, no seed) nothing is sent and the next tick retries. */
void RwaNtripClient::sendGga(bool force)
{
    if (!m_socket || m_phase != Phase::Streaming)
        return;
    if (!force && m_ggaSentThisSession && m_lastGgaSent.elapsed() < ggaIntervalMs)
        return;
    QString sentence = m_assemblyGga;
    if (sentence.isEmpty() && m_ggaSeed)
        sentence = m_ggaSeed();
    if (sentence.isEmpty())
        return;
    m_ggaSentThisSession = true;
    m_lastGgaSent.start();
    m_socket->write((sentence + QStringLiteral("\r\n")).toUtf8());
}

/** Ends the current attempt or session and schedules the next one. */
void RwaNtripClient::fail(const QString &reason, bool backoff)
{
    qWarning() << "NTRIP:" << reason;
    emit errorOccurred(reason);
    if (backoff) {
        m_attemptDelayMs = m_reconnectDelayMs;
        m_reconnectDelayMs = qMin(m_reconnectDelayMs * 2, backoffMaxMs);
    }
    closeSession();
    openSession();
}

void RwaNtripClient::closeSession()
{
    cancelDeadline();
    m_tick->stop();
    if (m_socket) {
        QTcpSocket *socket = m_socket;
        m_socket = nullptr;
        socket->disconnect(this);   // no afterglow from abort()
        socket->abort();
        socket->deleteLater();
    }
    m_phase = Phase::Idle;
    m_responseBuffer.clear();
    m_ggaSentThisSession = false;
    if (m_wasConnected) {
        m_wasConnected = false;
        report(State::Disconnected);
    }
}

void RwaNtripClient::report(State state)
{
    Status status;
    status.state = state;
    status.reconnects = qMax(0, m_successfulConnects - 1);
    status.bytesRx = m_bytesRx;
    emit statusChanged(status);
}

/** ***************************************** Timers ***************************************** */

void RwaNtripClient::armDeadline(int ms, const QString &reason)
{
    m_deadlineReason = reason;
    m_deadline->start(ms);
}

void RwaNtripClient::cancelDeadline()
{
    m_deadline->stop();
}

void RwaNtripClient::deadlineFired()
{
    fail(m_deadlineReason, true);
}
