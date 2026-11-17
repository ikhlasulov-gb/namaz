#include "TrackerNotifications.h"

#include "Tracker.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QStringList>
#include <QVariantMap>

namespace {
constexpr const char *kService = "org.freedesktop.Notifications";
constexpr const char *kPath = "/org/freedesktop/Notifications";
constexpr const char *kInterface = "org.freedesktop.Notifications";
}  // namespace

TrackerNotifications::TrackerNotifications(QObject *parent) : QObject(parent) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    if (!bus.interface()
             || !bus.interface()->isServiceRegistered(QString::fromLatin1(kService))) {
        return;
    }
    m_interface = new QDBusInterface(QString::fromLatin1(kService),
                                     QString::fromLatin1(kPath),
                                     QString::fromLatin1(kInterface),
                                     QDBusConnection::sessionBus(), this);
    if (!m_interface->isValid()) {
        delete m_interface;
        m_interface = nullptr;
        return;
    }

    const QDBusReply<QStringList> capabilities =
        m_interface->call(QStringLiteral("GetCapabilities"));
    m_actionsSupported = capabilities.isValid()
        && capabilities.value().contains(QStringLiteral("actions"));

    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kInterface), QStringLiteral("ActionInvoked"),
                this, SLOT(onActionInvoked(uint, QString)));
    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kInterface),
                QStringLiteral("NotificationClosed"), this,
                SLOT(onClosed(uint, uint)));
}

void TrackerNotifications::setTracker(PrayerTracker *tracker) {
    if (!tracker) {
        return;
    }
    connect(tracker, &PrayerTracker::due, this,
            [this, tracker](const QString &prayer, const QDate &date,
                            bool witr) {
                notify(prayer, date, witr, tracker->notifyButtons());
            });
}

void TrackerNotifications::notify(const QString &prayer, const QDate &date,
                                  bool witr, bool withButtons) {
    const int index = PrayerTracker::prayerIndex(prayer);
    const QString title = tr("Namaz");
    const QString body =
        witr
            ? tr("The last third of the night has begun: it is time for Witr.")
            : tr("It is time to pray %1.")
                  .arg(PrayerTracker::prayerName(index));

    if (!m_interface || !m_actionsSupported) {
        emit fallback(title, body, prayer, date, witr);
        return;
    }

    QStringList actions;
    if (withButtons) {
        actions << QStringLiteral("confirm") << tr("Confirm")
                << QStringLiteral("skip") << tr("Skip");
    }
    QVariantMap hints;
    hints.insert(QStringLiteral("urgency"), 1);
    hints.insert(QStringLiteral("desktop-entry"),
                 QStringLiteral("io.github.ikhlasulov_gb.Namaz"));

    const QDBusMessage message = m_interface->call(
        QStringLiteral("Notify"), QStringLiteral("Namaz"), 0u,
        QStringLiteral("io.github.ikhlasulov_gb.Namaz"), title, body, actions,
        hints, 0);
    const QDBusReply<uint> reply(message);
    if (!reply.isValid() || reply.value() == 0) {
        emit fallback(title, body, prayer, date, witr);
        return;
    }
    m_items.insert(reply.value(), {prayer, date, witr});
}

void TrackerNotifications::onActionInvoked(uint id, const QString &key) {
    if (!m_items.contains(id)) {
        return;
    }
    const Item item = m_items.take(id);
    if (key == QLatin1String("confirm")) {
        emit confirmRequested(item.prayer, item.date);
    } else if (key == QLatin1String("skip")) {
        emit skipRequested(item.prayer, item.date);
    }
}

void TrackerNotifications::onClosed(uint id, uint) {
    m_items.remove(id);
}
