/*
 * This file is part of the Rwa Creator.
 * An open-source cross-platform Middleware for creating interactive Soundwalks
 *
 * License: MIT
 *
 * rwantripclient.h
 *
 * NTRIP client. Since rtk-rover 0.48.0 the assembly has no radio but BLE, so
 * the central holds the caster session and proxies the correction loop: RTCM
 * from the caster goes down to the assembly (DeviceHandler's RTCM writer,
 * 713d0006), the assembly's own GGA goes up to the caster (713d0007, forwarded
 * every 10 s), seeded from the hero's map position until the receiver has a
 * fix.
 *
 * Raw TCP on QTcpSocket: casters answer "ICY 200 OK", which is not HTTP,
 * and stream forever, so QNetworkAccessManager is out. The session logic
 * mirrors rwa-player's NtripClient.swift (itself a mirror of the <= 0.47
 * firmware's task_rtk_get_corrrection_data, which ran against this caster
 * for weeks): one request per connection (re-sending with wrong settings
 * gets the account banned), a bounded response wait, a 30 s no-RTCM grace
 * after connect (VRS spin-up) that tightens to 10 s once data flows, and
 * a 5 s -> 60 s doubling backoff on failed or dataless attempts that
 * received data resets. Status is reported on transitions only.
 *
 * Single-threaded: lives on the thread that creates it (the GUI thread);
 * QTcpSocket delivers its signals there.
 */

#ifndef RWANTRIPCLIENT_H
#define RWANTRIPCLIENT_H

#include "rwacastersettings.h"

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QString>
#include <functional>

class QTcpSocket;
class QTimer;

/** Protocol pieces, pure functions (no socket): the request line, the
 *  response classification, the GGA seed sentence. */
namespace RwaNtrip {

/** The one request of a session: NTRIP 1.0 style GET with Basic auth.
 *  The User-Agent must start with "NTRIP" (caster requirement). */
QByteArray request(const RwaCasterSettings &settings, const QString &appVersion);

enum class Response {
    StreamOpen,    ///< "ICY 200 OK" or "HTTP/1.x 200": the bytes after the header are RTCM.
    SourceTable,   ///< "SOURCETABLE 200 OK" (or an HTTP 200 carrying a source table): unknown mount point.
    Unauthorized,  ///< 401: bad credentials or a ban.
    Other          ///< anything else; statusLine says what.
};

struct ParsedResponse {
    Response response = Response::Other;
    QString statusLine;
    int headerLength = 0;   ///< bytes to strip before the stream
};

/** Parses the response header block at the start of buffer; false while it
 *  is incomplete. An HTTP-style reply ("HTTP/...", "SOURCETABLE ...") ends
 *  at its first blank line. An ICY reply is its status line alone: some
 *  casters append the blank line, some don't, and waiting for it would
 *  deadlock against a VRS that streams nothing before our GGA. A blank
 *  line already in the buffer right after the status line is consumed
 *  here; one that arrives later is stripped from the stream's first bytes
 *  by the client. */
bool parseResponse(const QByteArray &buffer, ParsedResponse &out);
Response classify(const QString &statusLine, const QString &header);

/** A GGA sentence for a position the central knows (the hero on the map):
 *  the seed the caster gets until the assembly delivers its own. Quality 1, 8 satellites and HDOP 1.0 are placeholders; the VRS
 *  only needs the position to within a few km. No trailing CRLF. */
QString gga(double latitude, double longitude, double altitude, const QDateTime &utc);

/** NMEA checksum: XOR of every byte between '$' and '*', two uppercase hex digits. */
QString checksum(const QString &body);
bool hasValidChecksum(const QString &sentence);

/** "ddmm.mmmmm" / "dddmm.mmmmm". Computed in 1e-5-minute units so the
 *  minutes can never round up to 60.00000. */
QString degreesMinutes(double degrees, int degreeDigits);

} // namespace RwaNtrip

class RwaNtripClient : public QObject
{
    Q_OBJECT
public:
    /** The PROJECT-PLAN §4.3 ntrip_status states. Reconnecting is reported
     *  once per outage, only after a previous successful connect. */
    enum class State { Disconnected, Connected, Reconnecting };
    static QString stateName(State s);

    struct Status {
        State state = State::Disconnected;
        int reconnects = 0;     ///< successful connects - 1
        qint64 bytesRx = 0;     ///< RTCM bytes received from the caster, cumulative
    };

    // Tunables, mirrored from the <= 0.47 firmware (RTKRoverConfig.h) via NtripClient.swift.
    static constexpr int connectTimeoutMs  = 15000;
    static constexpr int responseTimeoutMs = 10000;   // CONNECTION_TIMEOUT_MS
    static constexpr int connectGraceMs    = 30000;   // NTRIP_CONNECT_GRACE_MS
    static constexpr int rtcmTimeoutMs     = 10000;   // NTRIP_RTCM_TIMEOUT_MS
    static constexpr int backoffStartMs    = 5000;    // NTRIP_BACKOFF_START_MS
    static constexpr int backoffMaxMs      = 60000;   // NTRIP_BACKOFF_MAX_MS
    static constexpr int ggaIntervalMs     = 10000;
    static constexpr int maxHeaderLength   = 4096;

    RwaNtripClient(const RwaCasterSettings &settings, const QString &appVersion,
                   QObject *parent = nullptr);
    ~RwaNtripClient() override;

    const RwaCasterSettings &settings() const { return m_settings; }

    /** A GGA seed (no CRLF) while the assembly has delivered none; an empty
     *  string when the central has no position either. */
    void setGgaSeed(std::function<QString()> seed) { m_ggaSeed = std::move(seed); }

    void start();
    /** Ends the session; reports Disconnected if one was open. The client
     *  is single-use: build a new one to start again. */
    void stop();

    /** The assembly's own GGA (713d0007). Sticky: once one exists it is
     *  what the caster gets, even if the receiver later loses its fix (a
     *  stale real position beats a fresh seed for the VRS). */
    void updateAssemblyGga(const QString &sentence);

signals:
    /** RTCM as it arrives, in order. */
    void rtcmReceived(const QByteArray &data);
    /** State transitions only. */
    void statusChanged(const RwaNtripClient::Status &status);
    /** The cumulative RTCM byte count, once per second while streaming and
     *  only when it moved (for the stats view). */
    void progress(qint64 bytesRx);
    /** The reason for every failed or dropped session. */
    void errorOccurred(const QString &reason);

private slots:
    void socketConnected();
    void socketReadyRead();
    void socketError();
    void socketDisconnected();
    void deadlineFired();
    void tickFired();

private:
    enum class Phase { Idle, Connecting, AwaitingResponse, Streaming };

    void openSession();
    void connectToCaster();
    void sendRequest();
    void received(const QByteArray &data);
    void streamOpened();
    void streamBytes(const QByteArray &data);
    void sendGga(bool force);
    void fail(const QString &reason, bool backoff);
    void closeSession();
    void report(State state);
    void armDeadline(int ms, const QString &reason);
    void cancelDeadline();

    RwaCasterSettings m_settings;
    QString m_appVersion;
    std::function<QString()> m_ggaSeed;

    QTcpSocket *m_socket = nullptr;
    QTimer *m_deadline = nullptr;
    QTimer *m_tick = nullptr;
    QTimer *m_attempt = nullptr;
    QString m_deadlineReason;

    bool m_running = false;
    Phase m_phase = Phase::Idle;
    QByteArray m_responseBuffer;
    bool m_stripLeadingBlankLine = false;
    QElapsedTimer m_lastData;
    bool m_gotDataThisSession = false;
    int m_reconnectDelayMs = backoffStartMs;
    int m_attemptDelayMs = 0;
    int m_successfulConnects = 0;
    bool m_wasConnected = false;
    bool m_reconnectingReported = false;
    qint64 m_bytesRx = 0;
    qint64 m_bytesRxReported = 0;
    QString m_assemblyGga;
    QElapsedTimer m_lastGgaSent;
    bool m_ggaSentThisSession = false;
};

Q_DECLARE_METATYPE(RwaNtripClient::Status)

#endif // RWANTRIPCLIENT_H
