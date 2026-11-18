#pragma once

#include <QDate>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

// Prayer order used everywhere in the tracker.  Witr is a tracker-only
// pseudo-prayer; the Rust core knows nothing about it.
namespace PrayerIndex {
constexpr int Fajr = 0;
constexpr int Dhuhr = 1;
constexpr int Asr = 2;
constexpr int Maghrib = 3;
constexpr int Isha = 4;
constexpr int Witr = 5;
constexpr int Count = 6;
}  // namespace PrayerIndex

namespace TrackerStatus {
inline constexpr char Pending[] = "pending";
inline constexpr char Confirmed[] = "confirmed";
inline constexpr char Auto[] = "auto";
inline constexpr char Skipped[] = "skipped";
inline constexpr char Missed[] = "missed";
}  // namespace TrackerStatus

// One prayer window on one night/day.
struct PrayerWindow {
    bool available = false;
    qint64 start = 0;     // when Confirm becomes valid
    qint64 notify = 0;    // reminder moment (start, or last third for Witr)
    qint64 deadline = 0;  // pending becomes missed after this
};

struct PrayerDay {
    QDate date;
    PrayerWindow prayers[PrayerIndex::Count];
};

// A persisted journal record.  `date` is the date of the night the prayer
// belongs to (Ishaa/Witr keep the previous day after midnight).
struct TrackerEntry {
    QString id;
    QString prayer;
    QDate date;
    QString scheduled;      // ISO snapshot, never rewritten
    qint64 start = 0;
    qint64 notify = 0;
    qint64 deadline = 0;
    QString status = QStringLiteral("pending");
    QString confirmedAt;    // ISO, empty while pending
    QString note;
    bool qada = false;      // a "make up" record that reduces the debt
    bool notified = false;  // reminder already shown for this record

    bool pending() const { return status == QLatin1String(TrackerStatus::Pending); }
    bool done() const {
        return status == QLatin1String(TrackerStatus::Confirmed)
            || status == QLatin1String(TrackerStatus::Auto);
    }
};

// Injectable prayer times.  The production implementation reads the day JSON
// of the Rust core; tests use a deterministic fake.
class PrayerTimeSource {
public:
    virtual ~PrayerTimeSource() = default;
    virtual QJsonObject dayJson(const QDate &date) const = 0;
};

class CorePrayerTimeSource : public PrayerTimeSource {
public:
    explicit CorePrayerTimeSource(void *core);
    QJsonObject dayJson(const QDate &date) const override;

private:
    void *m_core;
};

class PrayerTracker : public QObject {
    Q_OBJECT
public:
    explicit PrayerTracker(PrayerTimeSource *source, QObject *parent = nullptr);

    static QString defaultStoragePath();
    static QString prayerKey(int index);
    static int prayerIndex(const QString &key);
    static QString prayerName(int index);
    static QString prayerDescription(int index);
    static QString statusLabel(const QString &status);
    // Moment shown for a record: the last third of the night for Witr (the
    // time of its reminder), the prayer start for the obligatory prayers.
    static qint64 displayEpoch(const TrackerEntry &entry);
    // Runtime mask for `namaz_core_set_tracker_mask`: bits use the core's
    // time indices (Fajr 0, Dhuhr 2, Asr 3, Maghrib 4, Ishaa 5).
    static unsigned int coreNotificationMask(const PrayerTracker &tracker,
                                             bool globalNotifications);

    void setStoragePath(const QString &path);
    QString storagePath() const { return m_path; }
    // Tests inject a fixed clock.  Production uses the system clock.
    void setNowProvider(std::function<QDateTime()> provider);
    qint64 nowEpoch() const;

    bool load();
    bool save();
    void reset();

    QJsonObject settings() const { return m_settings; }
    void setSettings(const QJsonObject &settings);
    bool enabled() const;
    bool notifyEnabled(int index) const;
    bool autoConfirm(int index) const;
    // Witr is optional: it is tracked only when its own switch (or one of
    // its reminder switches, for compatibility) is enabled.
    bool witrEnabled() const;
    bool tracked(int index) const;
    bool notifyButtons() const;
    int historyDays() const;
    int qadaReminder() const;
    QString prayerNotes(int index) const;

    qint64 initialQada(int index) const;
    void setInitialQada(int index, qint64 value);
    qint64 qadaDebt(int index) const;

    // Creates today's records (and fills the days the application was closed)
    // and moves pending records to missed/auto at their deadline.
    bool tick();
    void ensureToday();

    QList<TrackerEntry> entriesFor(const QDate &date) const;
    QList<TrackerEntry> yesterdayPending(const QDate &today) const;
    QList<TrackerEntry> allEntries() const;
    QList<TrackerEntry> history(const QString &search = QString()) const;
    TrackerEntry entry(const QString &id) const;
    // Prayer windows for a date (used to render unavailable prayers grey).
    PrayerDay windowsFor(const QDate &date) const { return timesFor(date); }

    bool confirm(const QString &id);
    bool skip(const QString &id);
    bool isConfirmable(const TrackerEntry &entry) const;
    bool updateEntry(const TrackerEntry &entry);
    bool removeEntry(const QString &id);
    bool addQada(const QString &prayer, const QDate &date, const QString &note);

    bool pruneHistory();
    bool exportCsv(const QString &path) const;

signals:
    // Emitted when records or counters changed in a way the UI must redraw.
    void changed();
    // Emitted once per record when its reminder time is reached.  `missed`
    // names the prayers whose window ended at the same moment, so the new
    // prayer's notification can replace the old one and report it as missed.
    void due(const QString &prayer, const QDate &date, bool witr,
             const QStringList &missed);
    // Emitted when a pending prayer becomes missed at its deadline and there
    // is no new prayer notification to carry the message.
    void missed(const QString &prayer, const QDate &date, bool witr);
    // Emitted at most once per day when the qada debt reaches the threshold
    // set by `qada_reminder` (0 disables the reminder).
    void qadaDue(int index, qint64 debt);

private:
    PrayerDay timesFor(const QDate &date) const;
    bool ensureDay(const QDate &date, const QDateTime &now, bool backfill);
    QList<TrackerEntry> &dayList(const QDate &date);
    TrackerEntry *find(const QString &id);
    void fillDefaults();
    void emitChanged();

    PrayerTimeSource *m_source;
    std::function<QDateTime()> m_now;
    QString m_path;
    QJsonObject m_settings;
    qint64 m_qada[PrayerIndex::Count] = {};
    QMap<QDate, QList<TrackerEntry>> m_days;
    QDate m_lastDay;
    QDate m_lastQadaReminder;
    bool m_loading = false;
};
