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
    void dueSignalFiresOnce();
};

void TrackerTest::midnightKeepsIshaAndWitrInYesterdaySection() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FakeSource source;
    PrayerTracker tracker(&source);
    tracker.setStoragePath(dir.path() + QStringLiteral("/tracker.json"));
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
    qint64 now = at(day1(), 19, 0);
    tracker.setNowProvider(
        [&now] { return QDateTime::fromSecsSinceEpoch(now, Qt::UTC); });
    tracker.tick();

    QVERIFY(findEntry(tracker, day1(), QStringLiteral("witr")).id.isEmpty());
    QCOMPARE(tracker.entriesFor(day1()).size(), 5);
    QVERIFY(tracker.entriesFor(day1()).back().deadline > 0);
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

QTEST_MAIN(TrackerTest)
#include "tst_tracker.moc"
