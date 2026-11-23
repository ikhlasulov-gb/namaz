#pragma once

#include <QDate>
#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QString>

class QDBusInterface;

// Desktop notifications with Confirm/Skip actions over
// org.freedesktop.Notifications.  When the daemon is missing or does not
// support actions, `fallback` is emitted so the window can show its own
// banner instead of silently dropping the reminder.
class TrackerNotifications : public QObject {
    Q_OBJECT
public:
    explicit TrackerNotifications(QObject *parent = nullptr);

    bool actionsSupported() const { return m_actionsSupported; }
    // Posts the reminder.  Returns true when the notification daemon accepted
    // it; false means `fallback` was emitted and the journal must not treat the
    // reminder as delivered.
    bool notify(const QString &prayer, const QDate &date, bool witr,
                bool withButtons);
    // Dismiss every notification this process still owns.  Called on quit:
    // otherwise the daemon keeps the notification after the application's
    // D-Bus name is gone, leaving a message with dead action buttons.
    void closeAll();

signals:
    void confirmRequested(const QString &prayer, const QDate &date);
    void skipRequested(const QString &prayer, const QDate &date);
    // The desktop notification could not be used: show the in-window banner.
    void fallback(const QString &title, const QString &body,
                  const QString &prayer, const QDate &date, bool witr);

private slots:
    void onActionInvoked(uint id, const QString &key);
    void onClosed(uint id, uint reason);

private:
    struct Item {
        QString prayer;
        QDate date;
        bool witr = false;
    };

    // The notification daemon is acquired lazily and re-checked on every
    // beat: at autostart it may not be up yet, and plasmashell can restart
    // mid-session.  A failure is never cached permanently.
    static QDBusConnection bus();
    bool ensureInterface();
    bool ensureCapabilities();
    void daemonGone();
    bool fallbackNotify(const QString &prayer, const QDate &date, bool witr);

    QDBusInterface *m_interface = nullptr;
    bool m_actionsSupported = false;
    bool m_capabilitiesKnown = false;
    QHash<uint, Item> m_items;
};
