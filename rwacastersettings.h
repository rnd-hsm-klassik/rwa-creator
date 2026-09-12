/*
 * This file is part of the Rwa Creator.
 * An open-source cross-platform Middleware for creating interactive Soundwalks
 *
 * License: MIT
 *
 * rwacastersettings.h
 *
 * NTRIP caster settings. Since rtk-rover 0.48.0 the assembly has no radio but
 * BLE; whoever holds the BLE connection - the Player in the field, the Creator
 * on the desk - is the NTRIP client and proxies the correction loop. The caster
 * facts the firmware used to embed per unit live in QSettings ("caster/..."),
 * edited through Headtracker > NTRIP Caster... (RwaCasterDialog).
 *
 * The username is per unit (single-session accounts: a second client on
 * the same username starves the first); host, port, mount point and
 * password are shared across the fleet. Mirror of CasterSettings.swift.
 *
 * The password is stored in the QSettings plist like the other fields
 * (the Player made the same call with UserDefaults). Move it to the
 * Keychain if the Creator ever runs on shared machines.
 */

#ifndef RWACASTERSETTINGS_H
#define RWACASTERSETTINGS_H

#include <QString>

struct RwaCasterSettings
{
    static constexpr quint16 defaultPort = 2101;

    QString host;
    quint16 port = defaultPort;
    QString mount;
    QString user;
    QString pass;

    /** Everything a session needs. An empty password is allowed (sent as
     *  "user:"); host, port, mount point and username are not optional. */
    bool isComplete() const;

    /** "host:port/mount" for the stats view and logs. Never the credentials. */
    QString endpointDescription() const;

    bool operator==(const RwaCasterSettings &o) const;
    bool operator!=(const RwaCasterSettings &o) const { return !(*this == o); }

    static RwaCasterSettings load();
    void save() const;

    /** Empty -> the NTRIP default 2101; unparsable -> 0 (incomplete). */
    static quint16 parsePort(const QString &text);

    /** The request line is "GET /<mount>", so a leading slash typed by the
     *  operator is dropped rather than doubled. */
    static QString mountPoint(const QString &text);
};

#endif // RWACASTERSETTINGS_H
