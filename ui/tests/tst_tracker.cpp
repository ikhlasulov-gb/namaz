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
    void autoConfirmAvoidsDebt();
    void trackerFileRoundTrips();
    void failedSaveDoesNotCreateFile();
    void autoCleanupPreservesDebt();
    void csvExportContainsRows();
    void unavailableWitrIsNotTracked();
    void witrIsOptInAndUsesLastThird();
    void dueSignalFiresOnce();
    void newDayBackfillKeepsReminders();
    void lateStartInsideWindowNotifiesOnce();
    void wakeInsideWindowNotifies();
    void lateStartAfterWindowKeepsOnlyCurrentPrayer();
    void futureReminderMarkedShownIsHealed();
    void dueDoesNotAnnounceTheMissedPreviousPrayer();
    void missedPrayerIsNotAnnounced();
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

void TrackerTest::dueSignalFiresOnce() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 19, 59) + 58;
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    QSignalSpy spy(&tracker, &PrayerTracker::due);
    now = at(day1(), 20, 0);
    tracker.tick();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("isha"));

    now = at(day1(), 20, 0) + 2;
    tracker.tick();
    QCOMPARE(spy.count(), 1);
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
    // pending, and a prayer whose window is open is announced late.
    QSignalSpy spy(&tracker, &PrayerTracker::due);
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

    // Zuhr has not started yet, so its reminder must still be delivered.
    now = at(day1().addDays(1), 12, 0);
    tracker.tick();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("dhuhr"));

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
    // None of the missed prayers replaced the delivered Zuhr reminder.
    QCOMPARE(spy.count(), 1);
}

void TrackerTest::lateStartInsideWindowNotifiesOnce() {
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
    QSignalSpy spy(&tracker, &PrayerTracker::due);
    tracker.tick();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("dhuhr"));

    // One reminder per record: the next one-second tick stays quiet even
    // though the window is still open.
    tracker.tick();
    QCOMPARE(spy.count(), 1);

    // And a restart must not repeat it either.
    PrayerTracker loaded(&source);
    loaded.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    QVERIFY(loaded.load());
    loaded.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    QSignalSpy restarted(&loaded, &PrayerTracker::due);
    now += 60;
    loaded.tick();
    QCOMPARE(restarted.count(), 0);
}

void TrackerTest::wakeInsideWindowNotifies() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 11, 59);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    // The machine slept through the adhan and resumed inside the window.
    QSignalSpy spy(&tracker, &PrayerTracker::due);
    now = at(day1(), 12, 30);
    tracker.tick();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("dhuhr"));
}

void TrackerTest::lateStartAfterWindowKeepsOnlyCurrentPrayer() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));

    // Dhuhr's window ended at Asr; only the current prayer may be announced.
    qint64 now = at(day1(), 16, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    QSignalSpy spy(&tracker, &PrayerTracker::due);
    tracker.tick();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("asr"));
}

void TrackerTest::futureReminderMarkedShownIsHealed() {
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

    // The first tick must notice that the reminder moment is still ahead and
    // forget the stale "already shown" mark.
    QSignalSpy spy(&tracker, &PrayerTracker::due);
    tracker.tick();
    QVERIFY(!findEntry(tracker, today, QStringLiteral("dhuhr")).notified);
    QCOMPARE(spy.count(), 0);

    now = at(today, 12, 0);
    tracker.tick();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("dhuhr"));
}

void TrackerTest::dueDoesNotAnnounceTheMissedPreviousPrayer() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    QSignalSpy spy(&tracker, &PrayerTracker::due);
    // Ishaa begins exactly when Maghrib's window closes.
    now = at(day1(), 20, 0);
    tracker.tick();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("isha"));
    QCOMPARE(spy.at(0).size(), 3);
    // The record is still marked missed in the journal, but the notification
    // is only the regular Ishaa reminder.
    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("maghrib")).status,
             QStringLiteral("missed"));
}

void TrackerTest::missedPrayerIsNotAnnounced() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
    qint64 now = at(day1(), 5, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    QSignalSpy dueSpy(&tracker, &PrayerTracker::due);
    // Shuruq closes Fajr's window.  The record becomes missed, but no
    // "Missed" notification may replace the previous reminder.
    now = at(day1(), 6, 30) + 1;
    tracker.tick();
    QCOMPARE(findEntry(tracker, day1(), QStringLiteral("fajr")).status,
             QStringLiteral("missed"));
    QCOMPARE(dueSpy.count(), 0);

    // A machine waking hours later must not replay anything either.
    now = at(day1(), 7, 0);
    tracker.tick();
    QCOMPARE(dueSpy.count(), 0);
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
