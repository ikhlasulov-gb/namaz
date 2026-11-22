#include "Tracker.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTime>

namespace {

QDate day1() {
    return QDate(2026, 11, 18);
}

qint64 at(const QDate &date, int hour, int minute) {
    return QDateTime(date, QTime(hour, minute), Qt::UTC).toSecsSinceEpoch();
}

class FakeSource : public PrayerTimeSource {
public:
    bool witrAvailable = true;

    QJsonObject dayJson(const QDate &date) const override {
        QJsonArray times;
        const int hours[] = {4, 6, 12, 15, 18, 20};
        for (int index = 0; index < 6; ++index) {
            times.append(QJsonObject{
                {QStringLiteral("seconds"), hours[index] * 3600},
                {QStringLiteral("epoch"), at(date, hours[index], 0)},
            });
        }
        QJsonObject witr{
            {QStringLiteral("available"), witrAvailable},
            {QStringLiteral("epoch"), at(date.addDays(1), 1, 0)},
            {QStringLiteral("start_epoch"), at(date, 20, 0)},
            {QStringLiteral("deadline_epoch"), at(date.addDays(1), 5, 0)},
            {QStringLiteral("date"), date.addDays(1).toString(Qt::ISODate)},
        };
        return QJsonObject{
            {QStringLiteral("times"), times},
            {QStringLiteral("witr"), witr},
        };
    }
};

TrackerEntry findEntry(PrayerTracker &tracker, const QDate &date,
                       const QString &prayer, bool qada = false) {
    for (const TrackerEntry &entry : tracker.entriesFor(date)) {
        if (entry.prayer == prayer && entry.qada == qada) {
            return entry;
        }
    }
    return {};
}

void enableWitr(PrayerTracker &tracker) {
    QJsonObject settings = tracker.settings();
    settings.insert(QStringLiteral("witr"), true);
    tracker.setSettings(settings);
}

}  // namespace

class TrackerTest : public QObject {
    Q_OBJECT

private slots:
    void midnightKeepsIshaAndWitrInYesterdaySection();
    void confirmIsOnlyAllowedInsideTheWindow();
    void missedAtDeadlineAndSkippedIsNeutral();
    void qadaDebtFollowsHistoryEdits();
    void madeUpStatusIsNeutralAndPersists();
    void autoConfirmAvoidsDebt();
    void trackerFileRoundTrips();
    void failedSaveDoesNotCreateFile();
    void autoCleanupPreservesDebt();
    void csvExportContainsRows();
    void unavailableWitrIsNotTracked();
    void witrIsOptInAndUsesLastThird();
    void activeReminderTracksTheOpenWindow();
    void newDayBackfillKeepsReminders();
    void lateStartInsideWindowRepeatsUntilConfirmed();
    void wakeInsideWindowReminds();
    void lateStartAfterWindowKeepsOnlyCurrentPrayer();
    void staleNotifiedFlagDoesNotHideTheReminder();
    void onlyTheCurrentPrayerIsReminded();
    void missedPrayerIsNotReminded();
    void reminderRepeatCadence();
    void zeroRepeatShowsTheReminderOnce();
    void repeatSettingsHaveDefaultsAndPersist();
    void notificationMaskUsesCoreIndices();
    void qadaReminderFiresAtThreshold();
};

void TrackerTest::midnightKeepsIshaAndWitrInYesterdaySection() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    enableWitr(tracker);
    qint64 now = at(day1(), 21, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("isha")).status,
             QStringLiteral("pending"));
    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("witr")).status,
             QStringLiteral("pending"));
    QCOMPARE(tracker.yesterdayPending(day1().addDays(1)).size(), 2);

    now = at(day1().addDays(1), 1, 0);
    tracker.tick();
    QCOMPARE(tracker.yesterdayPending(day1().addDays(1)).size(), 2);
    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("isha")).date, day1());

    now = at(day1().addDays(1), 5, 0) + 1;
    tracker.tick();
    QCOMPARE(tracker.yesterdayPending(day1().addDays(1)).size(), 0);
    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("isha")).status,
             QStringLiteral("missed"));
    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("witr")).status,
             QStringLiteral("missed"));
}

void TrackerTest::confirmIsOnlyAllowedInsideTheWindow() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    enableWitr(tracker);
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    const TrackerEntry isha = findEntry(tracker, day1(), QStringLiteral("isha"));
    QVERIFY(!isha.id.isEmpty());
    QCOMPARE(isha.status, QStringLiteral("pending"));
    QVERIFY(!tracker.isConfirmable(isha));
    QVERIFY(!tracker.confirm(isha.id));

    // Witr may only be confirmed after Ishaa, even though its reminder is at
    // the last third of the night.
    const TrackerEntry witr = findEntry(tracker, day1(), QStringLiteral("witr"));
    QVERIFY(!tracker.isConfirmable(witr));

    now = at(day1(), 20, 0);
    QVERIFY(tracker.isConfirmable(isha));
    QVERIFY(tracker.isConfirmable(witr));
    QVERIFY(tracker.confirm(isha.id));
    QCOMPARE(tracker.entry(isha.id).status, QStringLiteral("confirmed"));

    // A missed Fajr can never be confirmed into the past.
    const TrackerEntry fajr = findEntry(tracker, day1(), QStringLiteral("fajr"));
    QCOMPARE(fajr.status, QStringLiteral("missed"));
    QVERIFY(!tracker.confirm(fajr.id));
}

void TrackerTest::missedAtDeadlineAndSkippedIsNeutral() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    // Fajr, Dhuhr, Asr and Maghrib are already over at 19:00.
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(1));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Dhuhr), qint64(1));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Asr), qint64(1));
    // Maghrib is still inside its window at 19:00.
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Maghrib), qint64(0));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Isha), qint64(0));

    const TrackerEntry isha = findEntry(tracker, day1(), QStringLiteral("isha"));
    now = at(day1(), 20, 0);
    QVERIFY(tracker.skip(isha.id));
    now = at(day1(), 21, 0);
    tracker.tick();
    QCOMPARE(tracker.entry(isha.id).status, QStringLiteral("skipped"));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Isha), qint64(0));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Maghrib), qint64(1));
}

void TrackerTest::qadaDebtFollowsHistoryEdits() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(1));
    QVERIFY(tracker.addQada(QStringLiteral("fajr"), day1(), QStringLiteral("late")));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(0));

    TrackerEntry qada = findEntry(tracker, day1(), QStringLiteral("fajr"), true);
    QVERIFY(!qada.id.isEmpty());
    QVERIFY(tracker.removeEntry(qada.id));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(1));

    const TrackerEntry missed = findEntry(tracker, day1(), QStringLiteral("fajr"));
    QVERIFY(tracker.removeEntry(missed.id));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(0));

    tracker.setInitialQada(PrayerIndex::Fajr, 3);
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(3));
}

void TrackerTest::madeUpStatusIsNeutralAndPersists() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/tracker.json");
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(path);
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(1));
    const TrackerEntry fajr = findEntry(tracker, day1(), QStringLiteral("fajr"));

    // Marking the missed prayer as made up neutralises its contribution to
    // the debt: the record carries the miss and its compensation together.
    TrackerEntry updated = fajr;
    updated.status = QLatin1String(TrackerStatus::MadeUp);
    QVERIFY(tracker.updateEntry(updated));
    QCOMPARE(tracker.entry(fajr.id).status,
             QLatin1String(TrackerStatus::MadeUp));
    QCOMPARE(tracker.entry(fajr.id).done(), true);
    QVERIFY(!tracker.entry(fajr.id).confirmedAt.isEmpty());
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(0));

    // Other prayers keep their debt.
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Dhuhr), qint64(1));

    // The status survives a round trip through the journal file.
    tracker.save();
    PrayerTracker loaded(&source);
    loaded.setStoragePath(path);
    QVERIFY(loaded.load());
    QCOMPARE(findEntry(loaded, day1(), QStringLiteral("fajr")).status,
             QLatin1String(TrackerStatus::MadeUp));
    QCOMPARE(loaded.qadaDebt(PrayerIndex::Fajr), qint64(0));

    // The history search matches the status label.
    QCOMPARE(loaded.history(QStringLiteral("made up")).size(), 1);
}

void TrackerTest::autoConfirmAvoidsDebt() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    QJsonObject settings = tracker.settings();
    QJsonObject autoConfirm =
        settings.value(QStringLiteral("auto_confirm")).toObject();
    autoConfirm.insert(QStringLiteral("fajr"), true);
    settings.insert(QStringLiteral("auto_confirm"), autoConfirm);
    tracker.setSettings(settings);

    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("fajr")).status,
             QStringLiteral("auto"));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(0));
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Dhuhr), qint64(1));
}

void TrackerTest::trackerFileRoundTrips() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/tracker.json");
    FakeSource source;
    {
        PrayerTracker tracker(&source);
        tracker.setStoragePath(path);
        enableWitr(tracker);
        qint64 now = at(day1(), 19, 0);
        tracker.setNowProvider(
            [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
        tracker.tick();
        QVERIFY(tracker.save());
    }
    QVERIFY(QFile::exists(path));

    PrayerTracker loaded(&source);
    loaded.setStoragePath(path);
    QVERIFY(loaded.load());
    QCOMPARE(loaded.entriesFor(day1()).size(), 6);
    QCOMPARE(findEntry(loaded, day1(), QStringLiteral("isha")).status,
             QStringLiteral("pending"));
    QCOMPARE(loaded.qadaDebt(PrayerIndex::Fajr), qint64(1));
}

void TrackerTest::failedSaveDoesNotCreateFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString blocker = dir.path() + QStringLiteral("/blocker");
    QFile file(blocker);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();

    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(blocker + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    QVERIFY(!tracker.save());
    QVERIFY(!QFile::exists(blocker + QStringLiteral("/tracker.json")));
}

void TrackerTest::autoCleanupPreservesDebt() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    QJsonObject settings = tracker.settings();
    settings.insert(QStringLiteral("history_days"), 0);
    tracker.setSettings(settings);

    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), qint64(1));

    now = at(day1().addDays(3), 8, 0);
    tracker.tick();
    const qint64 debtBefore = tracker.qadaDebt(PrayerIndex::Fajr);
    QVERIFY(debtBefore > 1);

    settings.insert(QStringLiteral("history_days"), 2);
    tracker.setSettings(settings);
    QVERIFY(tracker.pruneHistory());
    QVERIFY(tracker.entriesFor(day1()).isEmpty());
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Fajr), debtBefore);
    QCOMPARE(tracker.initialQada(PrayerIndex::Fajr), qint64(1));
}

void TrackerTest::csvExportContainsRows() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    const QString path = dir.path() + QStringLiteral("/journal.csv");
    QVERIFY(tracker.exportCsv(path));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(file.readAll());
    QVERIFY(text.startsWith(QStringLiteral("Date,Prayer,Status")));
    QVERIFY(text.contains(QStringLiteral("Fajr")));
    QVERIFY(text.contains(QStringLiteral("Missed")));
}

void TrackerTest::unavailableWitrIsNotTracked() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    source.witrAvailable = false;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    enableWitr(tracker);
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    QVERIFY(findEntry(tracker, day1(), QStringLiteral("witr")).id.isEmpty());
    QCOMPARE(tracker.entriesFor(day1()).size(), 5);
    QVERIFY(tracker.entriesFor(day1()).back().deadline > 0);
}

void TrackerTest::witrIsOptInAndUsesLastThird() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    // Witr is off by default: neither the table nor the qada counter sees it.
    QVERIFY(findEntry(tracker, day1(), QStringLiteral("witr")).id.isEmpty());
    QCOMPARE(tracker.entriesFor(day1()).size(), 5);
    QCOMPARE(tracker.qadaDebt(PrayerIndex::Witr), qint64(0));

    enableWitr(tracker);
    tracker.tick();
    const TrackerEntry witr = findEntry(tracker, day1(), QStringLiteral("witr"));
    QVERIFY(!witr.id.isEmpty());
    QCOMPARE(tracker.entriesFor(day1()).size(), 6);
    // The journal shows the last third of the night, not Ishaa's start.
    QCOMPARE(PrayerTracker::displayEpoch(witr),
             at(day1().addDays(1), 1, 0));
    QVERIFY(PrayerTracker::displayEpoch(witr) > witr.start);

    // Turning the option off hides an existing record again.
    QJsonObject settings = tracker.settings();
    settings.insert(QStringLiteral("witr"), false);
    QJsonObject notify = settings.value(QStringLiteral("notify")).toObject();
    notify.insert(QStringLiteral("witr"), false);
    settings.insert(QStringLiteral("notify"), notify);
    tracker.setSettings(settings);
    QVERIFY(findEntry(tracker, day1(), QStringLiteral("witr")).id.isEmpty());
    QCOMPARE(tracker.entriesFor(day1()).size(), 5);
}

void TrackerTest::activeReminderTracksTheOpenWindow() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 19, 59) + 58;
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    // Maghrib's window is still open, so its reminder is the active one.
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("maghrib"));

    // Ishaa begins exactly when Maghrib's window closes.
    now = at(day1(), 20, 0);
    tracker.tick();
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("isha"));

    // The reminder stays active for as long as the window is open: the UI, not
    // the tracker, decides how often to repeat it.
    now = at(day1(), 20, 2);
    tracker.tick();
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("isha"));

    // After Shuruq closes Fajr's window nothing is due until Zuhr.
    now = at(day1().addDays(1), 6, 30);
    tracker.tick();
    QVERIFY(tracker.activeReminder().id.isEmpty());
}

void TrackerTest::newDayBackfillKeepsReminders() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 20, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    // Namaz stayed closed overnight and starts again in the morning.  The new
    // day must not be treated as history: a reminder that is still ahead stays
    // pending, and a prayer whose window is open is reminded late.
    now = at(day1().addDays(1), 7, 0);
    tracker.ensureToday();
    const TrackerEntry fajr =
        findEntry(tracker, day1().addDays(1), QStringLiteral("fajr"));
    const TrackerEntry dhuhr =
        findEntry(tracker, day1().addDays(1), QStringLiteral("dhuhr"));
    QVERIFY(!fajr.id.isEmpty());
    QVERIFY(!dhuhr.id.isEmpty());
    // Fajr was missed while Namaz was not running: the closed window raises
    // no reminder.
    QCOMPARE(fajr.status, QStringLiteral("missed"));
    QVERIFY(!dhuhr.notified);
    QVERIFY(tracker.activeReminder().id.isEmpty());

    // Zuhr has not started yet, so its reminder must still be delivered.
    now = at(day1().addDays(1), 12, 0);
    tracker.tick();
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("dhuhr"));

    // The same prayer keeps being reminded while its window is open.
    now = at(day1().addDays(1), 12, 30);
    tracker.tick();
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("dhuhr"));

    // Days skipped entirely are closed history, but the current day is not.
    now = at(day1().addDays(3), 7, 0);
    tracker.tick();
    const TrackerEntry skipped =
        findEntry(tracker, day1().addDays(2), QStringLiteral("dhuhr"));
    QVERIFY(!skipped.id.isEmpty());
    QCOMPARE(skipped.status, QStringLiteral("missed"));
    const TrackerEntry nextDhuhr =
        findEntry(tracker, day1().addDays(3), QStringLiteral("dhuhr"));
    QVERIFY(!nextDhuhr.id.isEmpty());
    QVERIFY(!nextDhuhr.notified);
    // None of the missed prayers replaced the current reminder; at 07:00 the
    // next prayer (Zuhr) is still ahead, so nothing is due.
    QVERIFY(tracker.activeReminder().id.isEmpty());
}

void TrackerTest::lateStartInsideWindowRepeatsUntilConfirmed() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));

    // The computer was off and is switched on half an hour after the adhan,
    // while Zuhr is still current.
    qint64 now = at(day1(), 12, 30);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();
    const TrackerEntry first = tracker.activeReminder();
    QCOMPARE(first.prayer, QStringLiteral("dhuhr"));

    // A reminder the desktop accepted is not forgotten: the flag is
    // informational, and the record stays active so the UI can repeat it.
    QVERIFY(tracker.markNotified(first.id));
    QVERIFY(tracker.entry(first.id).notified);
    QCOMPARE(tracker.activeReminder().id, first.id);
    QVERIFY(!tracker.markNotified(first.id));

    // A restart must not lose it either.
    PrayerTracker loaded(&source);
    loaded.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    QVERIFY(loaded.load());
    loaded.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    now += 60;
    loaded.tick();
    QCOMPARE(loaded.activeReminder().id, first.id);

    // Confirming the prayer ends the reminder at once.
    QVERIFY(loaded.confirm(first.id));
    QVERIFY(loaded.activeReminder().id.isEmpty());
}

void TrackerTest::wakeInsideWindowReminds() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 11, 59);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();
    QVERIFY(tracker.activeReminder().id.isEmpty());

    // The machine slept through the adhan and resumed inside the window.
    now = at(day1(), 12, 30);
    tracker.tick();
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("dhuhr"));
}

void TrackerTest::lateStartAfterWindowKeepsOnlyCurrentPrayer() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));

    // Dhuhr's window ended at Asr; only the current prayer may be reminded.
    qint64 now = at(day1(), 16, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("asr"));
    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("dhuhr")).status,
             QStringLiteral("missed"));
}

void TrackerTest::staleNotifiedFlagDoesNotHideTheReminder() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/tracker.json");
    const QDate today = day1().addDays(1);

    // A tracker file written by 1.1.3 and older: the whole day was filled in
    // as history, so every reminder is marked as already shown even though
    // Zuhr is still ahead.
    const QJsonObject stale{
        {QStringLiteral("id"), QStringLiteral("stale-dhuhr")},
        {QStringLiteral("prayer"), QStringLiteral("dhuhr")},
        {QStringLiteral("date"), today.toString(Qt::ISODate)},
        {QStringLiteral("scheduled"),
         QDateTime::fromSecsSinceEpoch(at(today, 12, 0), Qt::UTC)
             .toString(Qt::ISODate)},
        {QStringLiteral("start_epoch"), at(today, 12, 0)},
        {QStringLiteral("notify_epoch"), at(today, 12, 0)},
        {QStringLiteral("deadline_epoch"), at(today, 15, 0)},
        {QStringLiteral("status"), QStringLiteral("pending")},
        {QStringLiteral("qada"), false},
        {QStringLiteral("notified"), true},
    };
    const QJsonObject root{
        {QStringLiteral("version"), 1},
        {QStringLiteral("last_day"), today.toString(Qt::ISODate)},
        {QStringLiteral("days"),
         QJsonObject{{today.toString(Qt::ISODate), QJsonArray{stale}}}},
    };
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(path);
    qint64 now = at(today, 11, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    QVERIFY(tracker.load());

    // The stale "already shown" mark must not matter: while the reminder
    // moment is still ahead nothing is due, and once it passes the record is
    // active again.
    tracker.tick();
    QVERIFY(tracker.activeReminder().id.isEmpty());

    now = at(today, 12, 0);
    tracker.tick();
    QCOMPARE(tracker.activeReminder().id, QStringLiteral("stale-dhuhr"));
}

void TrackerTest::onlyTheCurrentPrayerIsReminded() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("maghrib"));

    // Ishaa begins exactly when Maghrib's window closes.
    now = at(day1(), 20, 0);
    tracker.tick();
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("isha"));
    // The record is still marked missed in the journal, but the reminder is
    // only the regular Ishaa one.
    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("maghrib")).status,
             QStringLiteral("missed"));
}

void TrackerTest::missedPrayerIsNotReminded() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 5, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();
    QCOMPARE(tracker.activeReminder().prayer, QStringLiteral("fajr"));

    // Shuruq closes Fajr's window.  The record becomes missed and its reminder
    // must disappear instead of being repeated for the rest of the day.
    now = at(day1(), 6, 30) + 1;
    tracker.tick();
    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("fajr")).status,
             QStringLiteral("missed"));
    QVERIFY(tracker.activeReminder().id.isEmpty());

    // A machine waking hours later must not revive anything either.
    now = at(day1(), 7, 0);
    tracker.tick();
    QVERIFY(tracker.activeReminder().id.isEmpty());
}

void TrackerTest::reminderRepeatCadence() {
    ReminderRepeat repeat;
    const qint64 t0 = 1000000;
    // The first beat is immediate.
    QVERIFY(repeat.due(t0, 5, 12));
    QVERIFY(!repeat.started());
    repeat.take(t0, 5);
    QVERIFY(repeat.started());
    QCOMPARE(repeat.beats(), 1);

    // Four minutes later the reminder still waits; five is the cadence.
    QVERIFY(!repeat.due(t0 + 299, 5, 12));
    QVERIFY(repeat.due(t0 + 300, 5, 12));
    repeat.take(t0 + 300, 5);
    QCOMPARE(repeat.beats(), 2);

    // The limit stops the beeping even while the window is still open.
    for (int beat = 2; beat < 12; ++beat) {
        QVERIFY(repeat.due(t0 + 300 * (beat + 1), 5, 12));
        repeat.take(t0 + 300 * (beat + 1), 5);
    }
    QCOMPARE(repeat.beats(), 12);
    QVERIFY(!repeat.due(t0 + 300 * 40, 5, 12));

    // A new prayer window starts a fresh schedule.
    repeat.reset();
    QVERIFY(!repeat.started());
    QCOMPARE(repeat.beats(), 0);
    QVERIFY(repeat.due(t0 + 300 * 40, 5, 12));
}

void TrackerTest::zeroRepeatShowsTheReminderOnce() {
    // 0 minutes turns the repeats off: the reminder is shown once per window.
    ReminderRepeat repeat;
    QVERIFY(repeat.due(0, 0, 12));
    repeat.take(0, 0);
    QVERIFY(!repeat.due(3600, 0, 12));

    // A limit of one has the same effect.
    ReminderRepeat once;
    QVERIFY(once.due(0, 5, 1));
    once.take(0, 5);
    QVERIFY(!once.due(3600, 5, 1));
}

void TrackerTest::repeatSettingsHaveDefaultsAndPersist() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    QCOMPARE(tracker.repeatMinutes(), 5);
    QCOMPARE(tracker.repeatLimit(), 12);

    QJsonObject settings = tracker.settings();
    settings.insert(QStringLiteral("repeat_minutes"), 0);
    settings.insert(QStringLiteral("repeat_limit"), 3);
    tracker.setSettings(settings);
    QCOMPARE(tracker.repeatMinutes(), 0);
    QCOMPARE(tracker.repeatLimit(), 3);

    // The values survive a restart, so the cadence is a user setting and not a
    // build-time constant.
    PrayerTracker loaded(&source);
    loaded.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    QVERIFY(loaded.load());
    QCOMPARE(loaded.repeatMinutes(), 0);
    QCOMPARE(loaded.repeatLimit(), 3);

    // Negative values from a hand-edited file are clamped instead of breaking
    // the reminder loop.
    QJsonObject broken = loaded.settings();
    broken.insert(QStringLiteral("repeat_minutes"), -5);
    broken.insert(QStringLiteral("repeat_limit"), -1);
    loaded.setSettings(broken);
    QCOMPARE(loaded.repeatMinutes(), 0);
    QCOMPARE(loaded.repeatLimit(), 0);
}

void TrackerTest::qadaReminderFiresAtThreshold() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    QJsonObject settings = tracker.settings();
    settings.insert(QStringLiteral("qada_reminder"), 1);
    tracker.setSettings(settings);

    // Pin the clock inside the Fajr window (before Shuruq) so no prayer has
    // been missed yet; the system clock must not be consulted.
    qint64 now = at(day1(), 5, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });

    QSignalSpy spy(&tracker, &PrayerTracker::qadaDue);
    // No debt yet: the reminder must stay silent.
    tracker.tick();
    QCOMPARE(spy.count(), 0);

    // One missed prayer raises the debt to the threshold.
    now = at(day1(), 6, 30) + 1;  // Fajr window has ended (Shuruq).
    tracker.tick();
    QVERIFY(tracker.qadaDebt(PrayerIndex::Fajr) >= 1);
    QCOMPARE(spy.count(), 1);

    // At most one reminder per day.
    now += 3600;
    tracker.tick();
    QCOMPARE(spy.count(), 1);
}

void TrackerTest::notificationMaskUsesCoreIndices() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));

    // Defaults: five fards notify, Witr does not.
    QCOMPARE(PrayerTracker::coreNotificationMask(tracker, true),
             (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5));
    QCOMPARE(PrayerTracker::coreNotificationMask(tracker, false), 0u);

    QJsonObject settings = tracker.settings();
    QJsonObject notify = settings.value(QStringLiteral("notify")).toObject();
    notify.insert(QStringLiteral("asr"), true);
    notify.insert(QStringLiteral("fajr"), false);
    settings.insert(QStringLiteral("notify"), notify);
    settings.insert(QStringLiteral("enabled"), false);
    tracker.setSettings(settings);
    QCOMPARE(PrayerTracker::coreNotificationMask(tracker, true), 0u);

    settings.insert(QStringLiteral("enabled"), true);
    tracker.setSettings(settings);
    // Ishaa is always tracked now; only the per-prayer switches matter.
    QCOMPARE(PrayerTracker::coreNotificationMask(tracker, true),
             (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5));
}

QTEST_MAIN(TrackerTest)
#include "tst_tracker.moc"
