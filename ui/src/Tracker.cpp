#include "Tracker.h"

#include "Core.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QTime>
#include <QUuid>

#include <algorithm>

namespace {

const char *const kPrayerKeys[PrayerIndex::Count] = {
    "fajr", "dhuhr", "asr", "maghrib", "isha", "witr",
};

constexpr qint64 kNotifyGrace = 5;  // seconds, matches the core's strict window

QString tr(const char *text) {
    return QCoreApplication::translate("Tracker", text);
}

// statusText() lives in an anonymous namespace, where lupdate cannot see the
// literals; mark them for extraction and translate with the Tracker context.
const char *const kStatusTexts[] = {
    QT_TRANSLATE_NOOP("Tracker", "Pending"),
    QT_TRANSLATE_NOOP("Tracker", "Confirmed"),
    QT_TRANSLATE_NOOP("Tracker", "Auto-confirmed"),
    QT_TRANSLATE_NOOP("Tracker", "Skipped"),
    QT_TRANSLATE_NOOP("Tracker", "Missed"),
};

QJsonObject boolMap(std::initializer_list<bool> values) {
    QJsonObject map;
    int index = 0;
    for (bool value : values) {
        map.insert(QString::fromLatin1(kPrayerKeys[index++]), value);
    }
    return map;
}

QJsonObject mergeObjects(const QJsonObject &base, const QJsonObject &overrides) {
    QJsonObject result = base;
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        if (it.value().isObject() && result.value(it.key()).isObject()) {
            result.insert(it.key(), mergeObjects(result.value(it.key()).toObject(),
                                                 it.value().toObject()));
        } else {
            result.insert(it.key(), it.value());
        }
    }
    return result;
}

bool epochOf(const QJsonValue &value, qint64 &out) {
    if (!value.isDouble()) {
        return false;
    }
    out = static_cast<qint64>(value.toDouble());
    return true;
}

QString isoFromEpoch(qint64 epoch) {
    return QDateTime::fromSecsSinceEpoch(epoch).toString(Qt::ISODate);
}

QString csvQuote(const QString &value) {
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString statusText(const QString &status) {
    const char *key = nullptr;
    if (status == QLatin1String(TrackerStatus::Pending)) {
        key = kStatusTexts[0];
    } else if (status == QLatin1String(TrackerStatus::Confirmed)) {
        key = kStatusTexts[1];
    } else if (status == QLatin1String(TrackerStatus::Auto)) {
        key = kStatusTexts[2];
    } else if (status == QLatin1String(TrackerStatus::Skipped)) {
        key = kStatusTexts[3];
    } else if (status == QLatin1String(TrackerStatus::Missed)) {
        key = kStatusTexts[4];
    }
    return key ? tr(key) : status;
}

}  // namespace

CorePrayerTimeSource::CorePrayerTimeSource(void *core) : m_core(core) {}

QJsonObject CorePrayerTimeSource::dayJson(const QDate &date) const {
    if (!m_core || !date.isValid()) {
        return {};
    }
    const QString iso = date.toString(Qt::ISODate);
    char *raw = namaz_core_day_json_for_date(
        m_core, iso.toUtf8().constData());
    if (!raw) {
        return {};
    }
    const QString text = takeCoreString(raw);
    return QJsonDocument::fromJson(text.toUtf8()).object();
}

PrayerTracker::PrayerTracker(PrayerTimeSource *source, QObject *parent)
    : QObject(parent),
      m_source(source),
      m_now([] { return QDateTime::currentDateTime(); }),
      m_path(defaultStoragePath()) {
    fillDefaults();
}

QString PrayerTracker::defaultStoragePath() {
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share");
    }
    return base + QStringLiteral("/namaz/tracker.json");
}

void PrayerTracker::setStoragePath(const QString &path) {
    m_path = path;
}

void PrayerTracker::setNowProvider(std::function<QDateTime()> provider) {
    if (provider) {
        m_now = std::move(provider);
    }
}

qint64 PrayerTracker::nowEpoch() const {
    return m_now().toSecsSinceEpoch();
}

QString PrayerTracker::prayerKey(int index) {
    if (index < 0 || index >= PrayerIndex::Count) {
        return {};
    }
    return QString::fromLatin1(kPrayerKeys[index]);
}

int PrayerTracker::prayerIndex(const QString &key) {
    for (int index = 0; index < PrayerIndex::Count; ++index) {
        if (key == QLatin1String(kPrayerKeys[index])) {
            return index;
        }
    }
    return -1;
}

QString PrayerTracker::prayerName(int index) {
    switch (index) {
    case PrayerIndex::Fajr:
        return tr("Fajr");
    case PrayerIndex::Dhuhr:
        return tr("Dhuhr");
    case PrayerIndex::Asr:
        return tr("Asr");
    case PrayerIndex::Maghrib:
        return tr("Maghrib");
    case PrayerIndex::Isha:
        return tr("Ishaa");
    case PrayerIndex::Witr:
        return tr("Witr");
    default:
        return {};
    }
}

QString PrayerTracker::prayerDescription(int index) {
    switch (index) {
    case PrayerIndex::Fajr:
        return tr("2 sunnah + 2 fard");
    case PrayerIndex::Dhuhr:
        return tr("4 sunnah + 4 fard + 2 sunnah");
    case PrayerIndex::Asr:
        return tr("4 sunnah + 4 fard");
    case PrayerIndex::Maghrib:
        return tr("3 fard + 2 sunnah");
    case PrayerIndex::Isha:
        return tr("4 sunnah + 4 fard + 2 sunnah");
    case PrayerIndex::Witr:
        return tr("3 wajib, no adhan");
    default:
        return {};
    }
}

QString PrayerTracker::statusLabel(const QString &status) {
    return statusText(status);
}

unsigned int PrayerTracker::coreNotificationMask(const PrayerTracker &tracker,
                                                 bool globalNotifications) {
    if (!globalNotifications || !tracker.enabled()) {
        return 0;
    }
    // Tracker index -> core time index.  Witr has no core notification.
    static const int coreIndices[PrayerIndex::Count] = {0, 2, 3, 4, 5, -1};
    unsigned int mask = 0;
    for (int index = 0; index < PrayerIndex::Count; ++index) {
        const int core = coreIndices[index];
        if (core < 0) {
            continue;
        }
        // All six prayers are always tracked; the mask only follows the
        // per-prayer reminder switches.
        if (tracker.notifyEnabled(index)) {
            mask |= 1u << core;
        }
    }
    return mask;
}

void PrayerTracker::fillDefaults() {
    m_settings = QJsonObject{
        {QStringLiteral("enabled"), true},
        {QStringLiteral("notify"),
         boolMap({true, true, true, true, true, false})},
        {QStringLiteral("auto_confirm"),
         boolMap({false, false, false, false, false, false})},
        {QStringLiteral("notify_buttons"), true},
        {QStringLiteral("qada_reminder"), 0},
        {QStringLiteral("history_days"), 365},
    };
}

void PrayerTracker::setSettings(const QJsonObject &settings) {
    QJsonObject defaults = m_settings;
    // Always keep every known key so a partial settings object cannot break
    // the accessors.
    QJsonObject merged = mergeObjects(defaults, settings);
    m_settings = merged;
    save();
    emitChanged();
}

bool PrayerTracker::enabled() const {
    return m_settings.value(QStringLiteral("enabled")).toBool(true);
}

bool PrayerTracker::notifyEnabled(int index) const {
    return m_settings.value(QStringLiteral("notify"))
        .toObject()
        .value(QString::fromLatin1(kPrayerKeys[index]))
        .toBool(index != PrayerIndex::Witr);
}

bool PrayerTracker::autoConfirm(int index) const {
    return m_settings.value(QStringLiteral("auto_confirm"))
        .toObject()
        .value(QString::fromLatin1(kPrayerKeys[index]))
        .toBool(false);
}

bool PrayerTracker::notifyButtons() const {
    return m_settings.value(QStringLiteral("notify_buttons")).toBool(true);
}

int PrayerTracker::historyDays() const {
    return m_settings.value(QStringLiteral("history_days")).toInt(365);
}

int PrayerTracker::qadaReminder() const {
    return m_settings.value(QStringLiteral("qada_reminder")).toInt(0);
}

QString PrayerTracker::prayerNotes(int index) const {
    return m_settings.value(QStringLiteral("notes"))
        .toObject()
        .value(QString::fromLatin1(kPrayerKeys[index]))
        .toString();
}

qint64 PrayerTracker::initialQada(int index) const {
    if (index < 0 || index >= PrayerIndex::Count) {
        return 0;
    }
    return m_qada[index];
}

void PrayerTracker::setInitialQada(int index, qint64 value) {
    if (index < 0 || index >= PrayerIndex::Count) {
        return;
    }
    m_qada[index] = qMax<qint64>(0, value);
    save();
    emitChanged();
}

qint64 PrayerTracker::qadaDebt(int index) const {
    if (index < 0 || index >= PrayerIndex::Count) {
        return 0;
    }
    qint64 debt = m_qada[index];
    const QString key = prayerKey(index);
    for (const QList<TrackerEntry> &list : m_days) {
        for (const TrackerEntry &entry : list) {
            if (entry.prayer != key) {
                continue;
            }
            if (entry.qada && entry.status == QLatin1String(TrackerStatus::Confirmed)) {
                debt -= 1;
            } else if (!entry.qada
                       && entry.status == QLatin1String(TrackerStatus::Missed)) {
                debt += 1;
            }
        }
    }
    return qMax<qint64>(0, debt);
}

PrayerDay PrayerTracker::timesFor(const QDate &date) const {
    PrayerDay result;
    result.date = date;
    if (!m_source) {
        return result;
    }
    const QJsonObject day = m_source->dayJson(date);
    const QJsonArray times = day.value(QStringLiteral("times")).toArray();
    if (times.size() < 6) {
        return result;
    }

    auto timeEpoch = [&times](int index, qint64 &out) {
        if (index < 0 || index >= times.size()) {
            return false;
        }
        return epochOf(times.at(index).toObject().value(QStringLiteral("epoch")),
                       out);
    };

    qint64 shuruq = 0;
    qint64 dhuhr = 0;
    qint64 asr = 0;
    qint64 maghrib = 0;
    qint64 isha = 0;
    const bool hasShuruq = timeEpoch(1, shuruq);
    const bool hasDhuhr = timeEpoch(2, dhuhr);
    const bool hasAsr = timeEpoch(3, asr);
    const bool hasMaghrib = timeEpoch(4, maghrib);
    const bool hasIsha = timeEpoch(5, isha);

    qint64 fajr = 0;
    if (timeEpoch(0, fajr) && hasShuruq) {
        result.prayers[PrayerIndex::Fajr] = {true, fajr, fajr, shuruq};
    }
    if (hasDhuhr && hasAsr) {
        result.prayers[PrayerIndex::Dhuhr] = {true, dhuhr, dhuhr, asr};
    }
    if (hasAsr && hasMaghrib) {
        result.prayers[PrayerIndex::Asr] = {true, asr, asr, maghrib};
    }
    if (hasMaghrib && hasIsha) {
        result.prayers[PrayerIndex::Maghrib] = {true, maghrib, maghrib, isha};
    }

    const QJsonObject witr = day.value(QStringLiteral("witr")).toObject();
    qint64 witrEpoch = 0;
    qint64 witrStart = 0;
    qint64 witrDeadline = 0;
    const bool witrAvailable =
        witr.value(QStringLiteral("available")).toBool(false)
        && epochOf(witr.value(QStringLiteral("epoch")), witrEpoch)
        && epochOf(witr.value(QStringLiteral("start_epoch")), witrStart)
        && epochOf(witr.value(QStringLiteral("deadline_epoch")), witrDeadline);
    if (witrAvailable) {
        result.prayers[PrayerIndex::Witr] = {true, witrStart, witrEpoch,
                                             witrDeadline};
    }

    if (hasIsha) {
        qint64 nextFajr = 0;
        if (witrAvailable) {
            nextFajr = witrDeadline;
        } else if (m_source) {
            const QJsonObject next = m_source->dayJson(date.addDays(1));
            const QJsonArray nextTimes =
                next.value(QStringLiteral("times")).toArray();
            if (nextTimes.size() > 0) {
                epochOf(nextTimes.at(0).toObject().value(QStringLiteral("epoch")),
                        nextFajr);
            }
        }
        if (nextFajr > isha) {
            result.prayers[PrayerIndex::Isha] = {true, isha, isha, nextFajr};
        }
    }
    return result;
}

bool PrayerTracker::ensureDay(const QDate &date, const QDateTime &now,
                              bool backfill) {
    if (!date.isValid() || !m_source) {
        return false;
    }
    const PrayerDay windows = timesFor(date);
    const qint64 nowEpoch = now.toSecsSinceEpoch();
    QList<TrackerEntry> &list = dayList(date);

    QSet<QString> scheduled;
    for (const TrackerEntry &entry : list) {
        if (!entry.qada) {
            scheduled.insert(entry.prayer);
        }
    }

    bool added = false;
    for (int index = 0; index < PrayerIndex::Count; ++index) {
        const PrayerWindow &window = windows.prayers[index];
        if (!window.available) {
            continue;
        }
        const QString key = prayerKey(index);
        if (scheduled.contains(key)) {
            continue;
        }
        TrackerEntry entry;
        entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        entry.prayer = key;
        entry.date = date;
        entry.start = window.start;
        entry.notify = window.notify;
        entry.deadline = window.deadline;
        entry.scheduled = isoFromEpoch(window.start);
        if (nowEpoch >= window.deadline) {
            if (autoConfirm(index)) {
                entry.status = QLatin1String(TrackerStatus::Auto);
                entry.confirmedAt = isoFromEpoch(nowEpoch);
            } else {
                entry.status = QLatin1String(TrackerStatus::Missed);
            }
        } else {
            entry.status = QLatin1String(TrackerStatus::Pending);
        }
        // Never replay a reminder for a window that started before this
        // record existed (late start, wake from sleep, backfilled day).
        entry.notified = backfill || nowEpoch >= window.notify + kNotifyGrace;
        list.append(entry);
        added = true;
    }

    if (list.isEmpty()) {
        m_days.remove(date);
    }
    if (!m_lastDay.isValid() || date > m_lastDay) {
        m_lastDay = date;
    }
    return added;
}

QList<TrackerEntry> &PrayerTracker::dayList(const QDate &date) {
    return m_days[date];
}

void PrayerTracker::ensureToday() {
    if (!enabled()) {
        return;
    }
    const QDateTime now = m_now();
    const QDate today = now.date();
    bool added = false;
    if (!m_lastDay.isValid()) {
        added = ensureDay(today, now, false);
    } else if (m_lastDay < today) {
        QDate date = m_lastDay.addDays(1);
        int guard = 0;
        while (date <= today && guard++ < 400) {
            added = ensureDay(date, now, true) || added;
            date = date.addDays(1);
        }
    }
    added = ensureDay(today, now, false) || added;
    if (added) {
        const bool pruned = pruneHistory();
        if (!pruned) {
            save();
        }
        emitChanged();
    }
}

bool PrayerTracker::tick() {
    if (!enabled()) {
        return false;
    }
    // ensureToday() saves and emits when it created records.
    ensureToday();

    const qint64 nowEpoch = this->nowEpoch();
    bool changed = false;
    struct Due {
        QString prayer;
        QDate date;
        bool witr;
    };
    QList<Due> dueEvents;

    for (auto day = m_days.begin(); day != m_days.end(); ++day) {
        for (TrackerEntry &entry : day.value()) {
            if (entry.qada) {
                continue;
            }
            if (entry.pending() && nowEpoch >= entry.deadline) {
                const int index = prayerIndex(entry.prayer);
                if (index >= 0 && autoConfirm(index)) {
                    entry.status = QLatin1String(TrackerStatus::Auto);
                    entry.confirmedAt = isoFromEpoch(nowEpoch);
                } else {
                    entry.status = QLatin1String(TrackerStatus::Missed);
                }
                changed = true;
                continue;
            }
            if (entry.pending()) {
                const int index = prayerIndex(entry.prayer);
                if (index < 0) {
                    continue;
                }
                const qint64 at = entry.notify;
                if (!entry.notified && at > 0 && nowEpoch >= at
                    && nowEpoch <= at + kNotifyGrace) {
                    entry.notified = true;
                    if (notifyEnabled(index)) {
                        dueEvents.append({entry.prayer, entry.date,
                                          index == PrayerIndex::Witr});
                    }
                    changed = true;
                } else if (!entry.notified && at > 0
                           && nowEpoch > at + kNotifyGrace) {
                    entry.notified = true;
                    changed = true;
                }
            }
        }
    }

    if (changed) {
        save();
        emitChanged();
    }

    // Optional qada reminder: at most once per day, when the total debt of a
    // prayer reaches the configured threshold.
    const int threshold = qadaReminder();
    if (threshold > 0) {
        const QDate today = QDateTime::fromSecsSinceEpoch(nowEpoch).date();
        for (int index = 0; index < PrayerIndex::Count; ++index) {
            const qint64 debt = qadaDebt(index);
            if (debt >= threshold && m_lastQadaReminder != today) {
                m_lastQadaReminder = today;
                emit qadaDue(index, debt);
                break;
            }
        }
    }

    for (const Due &event : dueEvents) {
        emit due(event.prayer, event.date, event.witr);
    }
    return changed;
}

QList<TrackerEntry> PrayerTracker::entriesFor(const QDate &date) const {
    return m_days.value(date);
}

QList<TrackerEntry> PrayerTracker::yesterdayPending(const QDate &today) const {
    QList<TrackerEntry> result;
    const QList<TrackerEntry> list = m_days.value(today.addDays(-1));
    for (const TrackerEntry &entry : list) {
        if (entry.qada || !entry.pending()) {
            continue;
        }
        if (entry.prayer == QLatin1String("isha")
            || entry.prayer == QLatin1String("witr")) {
            result.append(entry);
        }
    }
    return result;
}

QList<TrackerEntry> PrayerTracker::allEntries() const {
    QList<TrackerEntry> result;
    for (auto day = m_days.constBegin(); day != m_days.constEnd(); ++day) {
        result.append(day.value());
    }
    std::sort(result.begin(), result.end(),
              [](const TrackerEntry &a, const TrackerEntry &b) {
                  if (a.date != b.date) {
                      return a.date > b.date;
                  }
                  return prayerIndex(a.prayer) < prayerIndex(b.prayer);
              });
    return result;
}

QList<TrackerEntry> PrayerTracker::history(const QString &search) const {
    QList<TrackerEntry> result;
    const QString needle = search.trimmed().toLower();
    for (const TrackerEntry &entry : allEntries()) {
        if (entry.pending()) {
            continue;
        }
        if (!needle.isEmpty()) {
            const QString haystack =
                (entry.date.toString(Qt::ISODate) + QLatin1Char(' ')
                 + prayerName(prayerIndex(entry.prayer)) + QLatin1Char(' ')
                 + entry.prayer + QLatin1Char(' ') + statusLabel(entry.status)
                 + QLatin1Char(' ') + entry.note)
                    .toLower();
            if (!haystack.contains(needle)) {
                continue;
            }
        }
        result.append(entry);
    }
    return result;
}

TrackerEntry PrayerTracker::entry(const QString &id) const {
    for (const QList<TrackerEntry> &list : m_days) {
        for (const TrackerEntry &entry : list) {
            if (entry.id == id) {
                return entry;
            }
        }
    }
    return {};
}

TrackerEntry *PrayerTracker::find(const QString &id) {
    for (auto day = m_days.begin(); day != m_days.end(); ++day) {
        for (TrackerEntry &entry : day.value()) {
            if (entry.id == id) {
                return &entry;
            }
        }
    }
    return nullptr;
}

bool PrayerTracker::isConfirmable(const TrackerEntry &entry) const {
    if (!entry.pending()) {
        return false;
    }
    const qint64 now = nowEpoch();
    return now >= entry.start && now < entry.deadline;
}

bool PrayerTracker::confirm(const QString &id) {
    TrackerEntry *entry = find(id);
    if (!entry || !isConfirmable(*entry)) {
        return false;
    }
    entry->status = QLatin1String(TrackerStatus::Confirmed);
    entry->confirmedAt = isoFromEpoch(nowEpoch());
    save();
    emitChanged();
    return true;
}

bool PrayerTracker::skip(const QString &id) {
    TrackerEntry *entry = find(id);
    if (!entry || !entry->pending() || nowEpoch() < entry->start) {
        return false;
    }
    entry->status = QLatin1String(TrackerStatus::Skipped);
    entry->confirmedAt = isoFromEpoch(nowEpoch());
    save();
    emitChanged();
    return true;
}

bool PrayerTracker::updateEntry(const TrackerEntry &updated) {
    TrackerEntry *entry = find(updated.id);
    if (!entry) {
        return false;
    }
    if (updated.status != QLatin1String(TrackerStatus::Pending)
        && updated.status != QLatin1String(TrackerStatus::Confirmed)
        && updated.status != QLatin1String(TrackerStatus::Auto)
        && updated.status != QLatin1String(TrackerStatus::Skipped)
        && updated.status != QLatin1String(TrackerStatus::Missed)) {
        return false;
    }
    entry->status = updated.status;
    entry->note = updated.note;
    if (entry->done()) {
        if (entry->confirmedAt.isEmpty()) {
            entry->confirmedAt = isoFromEpoch(nowEpoch());
        }
    } else if (entry->status == QLatin1String(TrackerStatus::Pending)) {
        entry->confirmedAt.clear();
    }
    save();
    emitChanged();
    return true;
}

bool PrayerTracker::removeEntry(const QString &id) {
    for (auto day = m_days.begin(); day != m_days.end(); ++day) {
        QList<TrackerEntry> &list = day.value();
        for (int index = 0; index < list.size(); ++index) {
            if (list.at(index).id == id) {
                list.removeAt(index);
                if (list.isEmpty()) {
                    m_days.erase(day);
                }
                save();
                emitChanged();
                return true;
            }
        }
    }
    return false;
}

bool PrayerTracker::addQada(const QString &prayer, const QDate &date,
                            const QString &note) {
    const int index = prayerIndex(prayer);
    if (index < 0 || !date.isValid()) {
        return false;
    }
    TrackerEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.prayer = prayer;
    entry.date = date;
    entry.qada = true;
    entry.status = QLatin1String(TrackerStatus::Confirmed);
    entry.confirmedAt = isoFromEpoch(nowEpoch());
    entry.scheduled = entry.confirmedAt;
    entry.start = nowEpoch();
    entry.deadline = entry.start;
    entry.note = note;
    entry.notified = true;
    dayList(date).append(entry);
    save();
    emitChanged();
    return true;
}

bool PrayerTracker::pruneHistory() {
    const int days = historyDays();
    if (days <= 0) {
        return false;
    }
    const QDate cutoff = m_now().date().addDays(-days);
    bool changed = false;
    for (auto day = m_days.begin(); day != m_days.end();) {
        if (day.key() >= cutoff) {
            ++day;
            continue;
        }
        // Keep the debt stable: fold the removed history into the manual
        // baseline before dropping it.
        for (const TrackerEntry &entry : day.value()) {
            const int index = prayerIndex(entry.prayer);
            if (index < 0) {
                continue;
            }
            if (entry.qada
                && entry.status == QLatin1String(TrackerStatus::Confirmed)) {
                m_qada[index] = qMax<qint64>(0, m_qada[index] - 1);
            } else if (!entry.qada
                       && entry.status == QLatin1String(TrackerStatus::Missed)) {
                m_qada[index] += 1;
            }
        }
        day = m_days.erase(day);
        changed = true;
    }
    if (changed) {
        save();
    }
    return changed;
}

bool PrayerTracker::exportCsv(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    QTextStream stream(&file);
    stream << "Date,Prayer,Status,Scheduled,Confirmed at,Qada,Note\n";
    for (const TrackerEntry &entry : allEntries()) {
        stream << csvQuote(entry.date.toString(Qt::ISODate)) << ','
               << csvQuote(prayerName(prayerIndex(entry.prayer))) << ','
               << csvQuote(statusLabel(entry.status)) << ','
               << csvQuote(entry.scheduled) << ','
               << csvQuote(entry.confirmedAt) << ','
               << (entry.qada ? 1 : 0) << ',' << csvQuote(entry.note) << '\n';
    }
    return true;
}

void PrayerTracker::reset() {
    m_days.clear();
    for (qint64 &value : m_qada) {
        value = 0;
    }
    m_lastDay = QDate();
    ensureToday();
    save();
    emitChanged();
}

bool PrayerTracker::load() {
    if (m_path.isEmpty() || !QFileInfo::exists(m_path)) {
        return false;
    }
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    const QJsonObject root = document.object();
    m_loading = true;

    m_settings = mergeObjects(m_settings,
                              root.value(QStringLiteral("settings")).toObject());
    // Versions before the vanilla-UI rework stored per-prayer colours,
    // icons, notes switches and enabled flags; the UI no longer uses them.
    for (const char *key : {"colors", "icons", "enabled_prayers"}) {
        m_settings.remove(QLatin1String(key));
    }

    const QJsonObject qada = root.value(QStringLiteral("qada")).toObject();
    for (int index = 0; index < PrayerIndex::Count; ++index) {
        const QString key = prayerKey(index);
        qint64 value = 0;
        if (epochOf(qada.value(key), value)) {
            m_qada[index] = qMax<qint64>(0, value);
        }
    }

    m_days.clear();
    const QJsonObject days = root.value(QStringLiteral("days")).toObject();
    for (auto day = days.begin(); day != days.end(); ++day) {
        const QDate date = QDate::fromString(day.key(), Qt::ISODate);
        if (!date.isValid()) {
            continue;
        }
        QList<TrackerEntry> list;
        for (const QJsonValue &value : day.value().toArray()) {
            const QJsonObject object = value.toObject();
            TrackerEntry entry;
            entry.id = object.value(QStringLiteral("id")).toString();
            if (entry.id.isEmpty()) {
                entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            }
            entry.prayer = object.value(QStringLiteral("prayer")).toString();
            if (prayerIndex(entry.prayer) < 0) {
                continue;
            }
            entry.date = QDate::fromString(
                object.value(QStringLiteral("date")).toString(), Qt::ISODate);
            if (!entry.date.isValid()) {
                entry.date = date;
            }
            entry.scheduled = object.value(QStringLiteral("scheduled")).toString();
            epochOf(object.value(QStringLiteral("start_epoch")), entry.start);
            epochOf(object.value(QStringLiteral("notify_epoch")), entry.notify);
            if (entry.notify <= 0) {
                entry.notify = entry.start;
            }
            epochOf(object.value(QStringLiteral("deadline_epoch")), entry.deadline);
            entry.status = object.value(QStringLiteral("status"))
                               .toString(QString::fromLatin1(TrackerStatus::Pending));
            entry.confirmedAt =
                object.value(QStringLiteral("confirmed_at")).toString();
            entry.note = object.value(QStringLiteral("note")).toString();
            entry.qada = object.value(QStringLiteral("qada")).toBool(false);
            entry.notified = object.value(QStringLiteral("notified")).toBool(false);
            list.append(entry);
        }
        if (!list.isEmpty()) {
            m_days.insert(date, list);
        }
    }

    const QDate last = QDate::fromString(
        root.value(QStringLiteral("last_day")).toString(), Qt::ISODate);
    m_lastDay = last;
    m_loading = false;
    return true;
}

bool PrayerTracker::save() {
    if (m_loading || m_path.isEmpty()) {
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("settings"), m_settings);

    QJsonObject qada;
    for (int index = 0; index < PrayerIndex::Count; ++index) {
        qada.insert(prayerKey(index), m_qada[index]);
    }
    root.insert(QStringLiteral("qada"), qada);

    QJsonObject days;
    for (auto day = m_days.constBegin(); day != m_days.constEnd(); ++day) {
        QJsonArray list;
        for (const TrackerEntry &entry : day.value()) {
            list.append(QJsonObject{
                {QStringLiteral("id"), entry.id},
                {QStringLiteral("prayer"), entry.prayer},
                {QStringLiteral("date"), entry.date.toString(Qt::ISODate)},
                {QStringLiteral("scheduled"), entry.scheduled},
                {QStringLiteral("start_epoch"), static_cast<double>(entry.start)},
                {QStringLiteral("notify_epoch"), static_cast<double>(entry.notify)},
                {QStringLiteral("deadline_epoch"),
                 static_cast<double>(entry.deadline)},
                {QStringLiteral("status"), entry.status},
                {QStringLiteral("confirmed_at"), entry.confirmedAt},
                {QStringLiteral("note"), entry.note},
                {QStringLiteral("qada"), entry.qada},
                {QStringLiteral("notified"), entry.notified},
            });
        }
        days.insert(day.key().toString(Qt::ISODate), list);
    }
    root.insert(QStringLiteral("days"), days);
    if (m_lastDay.isValid()) {
        root.insert(QStringLiteral("last_day"), m_lastDay.toString(Qt::ISODate));
    }

    const QFileInfo info(m_path);
    if (!QDir().mkpath(info.absolutePath())) {
        return false;
    }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

void PrayerTracker::emitChanged() {
    emit changed();
}
