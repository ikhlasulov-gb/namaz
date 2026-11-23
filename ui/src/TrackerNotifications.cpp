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
    // The interface is acquired lazily by `ensureInterface()`.  At startup the
    // notification daemon may not be there yet (Namaz is autostarted and often
    // comes up before plasmashell), and plasmashell can also restart later in
    // the session; a check made once in the constructor used to disable the
    // desktop notifications for the whole run and silently left every prayer
    // reminder on the hidden fallback banner.
    bus().connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                  QString::fromLatin1(kInterface), QStringLiteral("ActionInvoked"),
                  this, SLOT(onActionInvoked(uint, QString)));
    bus().connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                  QString::fromLatin1(kInterface),
                  QStringLiteral("NotificationClosed"), this,
                  SLOT(onClosed(uint, uint)));
}

QDBusConnection TrackerNotifications::bus() {
    return QDBusConnection::sessionBus();
}

bool TrackerNotifications::ensureInterface() {
    if (m_interface && m_interface->isValid()) {
        return true;
    }
    delete m_interface;
    m_interface = nullptr;

    if (!bus().isConnected()) {
        return false;
    }
    auto *interface = new QDBusInterface(QString::fromLatin1(kService),
                                         QString::fromLatin1(kPath),
                                         QString::fromLatin1(kInterface),
                                         bus(), this);
    // A short timeout keeps a wedged daemon from stalling the reminder beat.
    interface->setTimeout(5000);
    if (!interface->isValid()) {
        // The daemon is absent (not yet started, or restarting).  Do not
        // remember the failure: the next beat must try again.
        delete interface;
        return false;
    }
    m_interface = interface;
    return true;
}

bool TrackerNotifications::ensureCapabilities() {
    if (m_capabilitiesKnown) {
        return m_actionsSupported;
    }
    if (!ensureInterface()) {
        return false;
    }
    const QDBusReply<QStringList> capabilities =
        m_interface->call(QStringLiteral("GetCapabilities"));
    if (!capabilities.isValid()) {
        // The daemon may have disappeared between the interface check and
        // this call; retry on the next beat instead of caching the answer.
        delete m_interface;
        m_interface = nullptr;
        return false;
    }
    m_actionsSupported =
        capabilities.value().contains(QStringLiteral("actions"));
    m_capabilitiesKnown = true;
    return m_actionsSupported;
}

void TrackerNotifications::daemonGone() {
    // The notification daemon restarted (for example plasmashell was
    // replaced): drop the stale interface and the stale capability cache so
    // the next reminder reconnects instead of posting into a dead peer.
    delete m_interface;
    m_interface = nullptr;
    m_capabilitiesKnown = false;
    m_items.clear();
}

void TrackerNotifications::closeAll() {
    if (!ensureInterface()) {
        m_items.clear();
        return;
    }
    const QList<uint> ids = m_items.keys();
    m_items.clear();
    for (uint id : ids) {
        m_interface->call(QStringLiteral("CloseNotification"), id);
    }
}

bool TrackerNotifications::notify(const QString &prayer, const QDate &date,
                                  bool witr, bool withButtons) {
    if (!ensureInterface() || !ensureCapabilities()) {
        // No usable daemon right now (it may not have started yet, or it is
        // restarting).  Fall back to the banner for this beat only: the next
        // repeat tries the desktop notification again.
        return fallbackNotify(prayer, date, witr);
    }
    const int index = PrayerTracker::prayerIndex(prayer);
    const QString title = tr("Namaz");
    const QString body =
        witr
            ? tr("The last third of the night has begun: it is time for Witr.")
            : tr("It is time to pray %1.")
                  .arg(PrayerTracker::prayerName(index));

    QStringList actions;
    if (withButtons) {
        actions << QStringLiteral("confirm") << tr("Confirm")
                << QStringLiteral("skip") << tr("Skip");
    }
    QVariantMap hints;
    // Critical urgency: prayer reminders behave like alarms (the adhan is
    // already playing), and Plasma 6 suppresses normal-urgency popups while
    // a fullscreen window is focused or Do-Not-Disturb is enabled.  Critical
    // notifications are delivered in both cases.
    hints.insert(QStringLiteral("urgency"), 2);
    hints.insert(QStringLiteral("desktop-entry"),
                 QStringLiteral("io.github.ikhlasulov_gb.Namaz"));
    // Keep the prayer reminder on screen until it is confirmed, skipped or
    // closed instead of letting the daemon time it out.
    hints.insert(QStringLiteral("resident"), true);

    // A prayer reminder replaces the previous one; notifications must not
    // accumulate while the day moves from one prayer to the next.
    closeAll();
    const QDBusMessage message = m_interface->call(
        QStringLiteral("Notify"), QStringLiteral("Namaz"), 0u,
        QStringLiteral("io.github.ikhlasulov_gb.Namaz"), title, body, actions,
        hints, 0);
    const QDBusReply<uint> reply(message);
    if (!reply.isValid() || reply.value() == 0) {
        // The call failed (the daemon died mid-flight or rejected the
        // message): forget the cached state and let this beat use the
        // banner, while the next repeat reconnects.
        daemonGone();
        return fallbackNotify(prayer, date, witr);
    }
    m_items.insert(reply.value(), {prayer, date, witr});
    return true;
}

bool TrackerNotifications::fallbackNotify(const QString &prayer,
                                          const QDate &date, bool witr) {
    const int index = PrayerTracker::prayerIndex(prayer);
    const QString title = tr("Namaz");
    const QString body =
        witr
            ? tr("The last third of the night has begun: it is time for Witr.")
            : tr("It is time to pray %1.")
                  .arg(PrayerTracker::prayerName(index));
    // The fallback banner replaces its previous contents, so only one
    // prayer reminder is visible there as well.
    emit fallback(title, body, prayer, date, witr);
    return false;
}

void TrackerNotifications::onActionInvoked(uint id, const QString &key) {
    if (!m_items.contains(id)) {
        return;
    }
    const Item item = m_items.take(id);
    // Dismiss immediately instead of waiting for the daemon's own timeout.
    if (m_interface && m_interface->isValid()) {
        m_interface->call(QStringLiteral("CloseNotification"), id);
    }
    if (key == QLatin1String("confirm")) {
        emit confirmRequested(item.prayer, item.date);
    } else if (key == QLatin1String("skip")) {
        emit skipRequested(item.prayer, item.date);
    }
}

void TrackerNotifications::onClosed(uint id, uint) {
    m_items.remove(id);
}
