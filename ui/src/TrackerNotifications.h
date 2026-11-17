#pragma once

#include <QDate>
#include <QHash>
#include <QObject>
#include <QString>

class QDBusInterface;
class PrayerTracker;

// Desktop notifications with Confirm/Skip actions over
// org.freedesktop.Notifications.  When the daemon is missing or does not
// support actions, `fallback` is emitted so the window can show its own
// banner instead of silently dropping the reminder.
class TrackerNotifications : public QObject {
    Q_OBJECT
public:
    explicit TrackerNotifications(QObject *parent = nullptr);

    void setTracker(PrayerTracker *tracker);
    bool actionsSupported() const { return m_actionsSupported; }
    void notify(const QString &prayer, const QDate &date, bool witr,
                bool withButtons);

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

    QDBusInterface *m_interface = nullptr;
    bool m_actionsSupported = false;
    QHash<uint, Item> m_items;
};
