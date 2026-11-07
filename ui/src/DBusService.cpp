#include "DBusService.h"

#include "UiUtils.h"

#include <QDBusConnection>
#include <QJsonArray>

namespace {
constexpr const char *kServiceName = "io.github.ikhlasulov_gb.Namaz";
constexpr const char *kObjectPath = "/Namaz";
}  // namespace

DBusService::DBusService(QObject *parent) : QObject(parent) {}

bool DBusService::enable() {
    if (m_enabled) {
        return true;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        return false;
    }
    if (!bus.registerObject(QString::fromLatin1(kObjectPath), this,
                            QDBusConnection::ExportAllProperties
                                | QDBusConnection::ExportAllSlots)) {
        bus.unregisterService(QString::fromLatin1(kServiceName));
        return false;
    }
    m_enabled = true;
    return true;
}

void DBusService::disable() {
    if (!m_enabled) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    bus.unregisterService(QString::fromLatin1(kServiceName));
    m_enabled = false;
}

void DBusService::update(const QJsonObject &day, int intervalSeconds) {
    const QJsonObject now = day.value(QStringLiteral("now")).toObject();
    const QJsonObject next = day.value(QStringLiteral("next")).toObject();
    const QJsonObject qibla = day.value(QStringLiteral("qibla")).toObject();
    const QJsonObject date = day.value(QStringLiteral("date")).toObject();
    const QJsonArray times = day.value(QStringLiteral("times")).toArray();

    const QString nextName = next.value(QStringLiteral("name")).toString();
    const int nextSeconds = next.value(QStringLiteral("seconds")).toInt(-1);
    const QString nextTime = nextSeconds >= 0 ? Ui::formatTime(nextSeconds) : QString();
    const QString key = nextName + QLatin1Char('|') + nextTime;

    const QDateTime current = QDateTime::currentDateTime();
    if (intervalSeconds > 0 && key == m_lastKey && m_lastUpdate.isValid()
        && m_lastUpdate.secsTo(current) < intervalSeconds) {
        return;
    }

    m_nextPrayerName = nextName;
    m_nextPrayerTime = nextTime;
    const int nowSeconds = now.value(QStringLiteral("seconds")).toInt(-1);
    int remaining = 0;
    if (nextSeconds >= 0 && nowSeconds >= 0) {
        remaining = nextSeconds - nowSeconds;
        if (remaining < 0) {
            remaining += 24 * 3600;
        }
    }
    m_timeRemainingSeconds = remaining;
    m_timeRemaining = Ui::formatCountdown(remaining);
    for (int index = 0; index < 6; ++index) {
        const QJsonObject prayer = index < times.size()
                                       ? times.at(index).toObject()
                                       : QJsonObject();
        const int seconds = prayer.value(QStringLiteral("seconds")).toInt(-1);
        m_times[index] = seconds >= 0
                             ? Ui::formatTime(seconds)
                             : prayer.value(QStringLiteral("short")).toString();
    }
    m_qiblah = qibla.value(QStringLiteral("text")).toString();
    m_location = day.value(QStringLiteral("city")).toString();
    m_currentDate = date.value(QStringLiteral("line")).toString();
    m_lastKey = key;
    m_lastUpdate = current;
    emit changed();
}

QString DBusService::topicItemList() const {
    return QStringList{
        QStringLiteral("NextPrayerTime"),
        QStringLiteral("NextPrayerName"),
        QStringLiteral("TimeRemaining"),
        QStringLiteral("TimeRemainingSeconds"),
        QStringLiteral("Fajr"),
        QStringLiteral("Shuruq"),
        QStringLiteral("Dhuhr"),
        QStringLiteral("Asr"),
        QStringLiteral("Maghrib"),
        QStringLiteral("Ishaa"),
        QStringLiteral("Qiblah"),
        QStringLiteral("Location"),
        QStringLiteral("CurrentDate"),
    }.join(QLatin1Char('\t'));
}

void DBusService::Show() {
    emit showRequested();
}

void DBusService::Quit() {
    emit quitRequested();
}

QString DBusService::GetTopicItemList() const {
    return topicItemList();
}
