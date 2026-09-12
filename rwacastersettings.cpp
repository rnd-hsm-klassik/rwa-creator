#include "rwacastersettings.h"
#include <QSettings>

namespace {
const char *kHost  = "caster/host";
const char *kPort  = "caster/port";
const char *kMount = "caster/mount";
const char *kUser  = "caster/user";
const char *kPass  = "caster/pass";
}

bool RwaCasterSettings::isComplete() const
{
    return !host.isEmpty() && port > 0 && !mount.isEmpty() && !user.isEmpty();
}

QString RwaCasterSettings::endpointDescription() const
{
    return QStringLiteral("%1:%2/%3").arg(host).arg(port).arg(mount);
}

bool RwaCasterSettings::operator==(const RwaCasterSettings &o) const
{
    return host == o.host && port == o.port && mount == o.mount
        && user == o.user && pass == o.pass;
}

RwaCasterSettings RwaCasterSettings::load()
{
    QSettings settings;
    RwaCasterSettings s;
    s.host  = settings.value(kHost).toString().trimmed();
    s.port  = parsePort(settings.value(kPort).toString());
    s.mount = mountPoint(settings.value(kMount).toString());
    s.user  = settings.value(kUser).toString().trimmed();
    s.pass  = settings.value(kPass).toString();
    return s;
}

void RwaCasterSettings::save() const
{
    QSettings settings;
    settings.setValue(kHost, host.trimmed());
    settings.setValue(kPort, QString::number(port));
    settings.setValue(kMount, mountPoint(mount));
    settings.setValue(kUser, user.trimmed());
    settings.setValue(kPass, pass);
    settings.sync();
}

quint16 RwaCasterSettings::parsePort(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return defaultPort;
    bool ok = false;
    const uint value = t.toUInt(&ok);
    if (!ok || value == 0 || value > 65535)
        return 0;
    return quint16(value);
}

QString RwaCasterSettings::mountPoint(const QString &text)
{
    QString t = text.trimmed();
    while (t.startsWith(QLatin1Char('/')))
        t.remove(0, 1);
    return t;
}
