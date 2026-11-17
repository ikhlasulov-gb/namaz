#include "TrackerView.h"

#include "TrackerDialogs.h"
#include "UiUtils.h"

#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QString timeText(qint64 epoch) {
    if (epoch <= 0) {
        return QStringLiteral("N/A");
    }
    const QDateTime local = QDateTime::fromSecsSinceEpoch(epoch);
    return Ui::formatTime(local.time().msecsSinceStartOfDay() / 1000);
}

void clearLayout(QVBoxLayout *layout) {
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

// A plain QGroupBox section, exactly like the cards on the Day view.
QWidget *section(const QString &title, const QList<QWidget *> &rows,
                 QWidget *parent) {
    if (rows.isEmpty()) {
        return nullptr;
    }
    auto *box = new QGroupBox(title, parent);
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(Ui::pageMargin(), Ui::groupSpacing(),
                               Ui::pageMargin(), Ui::groupSpacing());
    layout->setSpacing(Ui::controlSpacing());
    for (QWidget *row : rows) {
        layout->addWidget(row);
    }
    return box;
}

// A borderless row: name and description on the left, then time, status and
// the Confirm/Skip buttons.  Only palette roles are used, so the rows follow
// the system Breeze theme like every other list in Namaz.
QWidget *plainRow(const QList<QWidget *> &widgets, int stretchIndex) {
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Ui::controlSpacing());
    for (int index = 0; index < widgets.size(); ++index) {
        layout->addWidget(widgets.at(index), index == stretchIndex ? 1 : 0);
    }
    return row;
}

}  // namespace

TrackerTodayPage::TrackerTodayPage(PrayerTracker *tracker, QWidget *parent)
    : QWidget(parent), m_tracker(tracker) {
    setObjectName(QStringLiteral("TrackerTodayPage"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(Ui::controlSpacing());

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("TrackerSummaryLabel"));
    Ui::styleCaption(m_summary);
    m_summary->setWordWrap(true);
    outer->addWidget(m_summary);

    // The only entry point for a manual make-up record.  It stays above the
    // scrolling list so it is reachable even when Today is long.
    m_makeUp = new QPushButton(tr("Make up a missed prayer…"), this);
    m_makeUp->setObjectName(QStringLiteral("TrackerQadaButton"));
    connect(m_makeUp, &QPushButton::clicked, this,
            &TrackerTodayPage::makeUpQada);
    outer->addWidget(m_makeUp);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    m_layout = new QVBoxLayout(content);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(Ui::groupSpacing() * 2);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
}

void TrackerTodayPage::clearContent() {
    clearLayout(m_layout);
}

void TrackerTodayPage::refresh() {
    clearContent();
    QWidget *content = m_layout->parentWidget();
    if (!m_tracker->enabled()) {
        auto *disabled = new QLabel(
            tr("The prayer journal is disabled. Enable it in Preferences → "
               "Journal."),
            content);
        disabled->setWordWrap(true);
        disabled->setForegroundRole(QPalette::PlaceholderText);
        m_layout->addWidget(disabled);
        m_layout->addStretch(1);
        m_summary->setText(tr("Journal disabled"));
        if (m_makeUp) {
            m_makeUp->setEnabled(false);
        }
        return;
    }
    if (m_makeUp) {
        m_makeUp->setEnabled(true);
    }

    const QDate today =
        QDateTime::fromSecsSinceEpoch(m_tracker->nowEpoch()).date();
    const qint64 now = m_tracker->nowEpoch();

    QStringList debts;
    for (int index = 0; index < PrayerIndex::Count; ++index) {
        const qint64 debt = m_tracker->qadaDebt(index);
        if (debt > 0) {
            debts.append(QStringLiteral("%1: %2")
                             .arg(PrayerTracker::prayerName(index))
                             .arg(debt));
        }
    }
    m_summary->setText(debts.isEmpty()
                           ? tr("No missed prayers in the qada counter.")
                           : tr("Qada debt — %1").arg(
                                 debts.join(QStringLiteral("  ·  "))));

    const QList<TrackerEntry> yesterday = m_tracker->yesterdayPending(today);
    const QList<TrackerEntry> todayEntries = m_tracker->entriesFor(today);

    QList<QWidget *> yesterdayRows;
    for (const TrackerEntry &entry : yesterday) {
        yesterdayRows.append(entryRow(entry, true));
    }

    QList<QWidget *> currentRows;
    QList<QWidget *> upcomingRows;
    QList<QWidget *> missedRows;
    QList<QWidget *> doneRows;
    QSet<QString> present;
    for (const TrackerEntry &entry : todayEntries) {
        if (!entry.qada) {
            present.insert(entry.prayer);
        }
        if (entry.pending()) {
            if (now >= entry.start) {
                currentRows.append(entryRow(entry, false));
            } else {
                upcomingRows.append(entryRow(entry, false));
            }
        } else if (entry.status == QLatin1String(TrackerStatus::Missed)) {
            missedRows.append(entryRow(entry, false));
        } else {
            doneRows.append(entryRow(entry, false));
        }
    }

    const PrayerDay windows = m_tracker->windowsFor(today);
    for (int index = 0; index < PrayerIndex::Count; ++index) {
        if (present.contains(PrayerTracker::prayerKey(index))) {
            continue;
        }
        if (!windows.prayers[index].available) {
            upcomingRows.append(unavailableRow(index));
        }
    }

    auto add = [this, content](const QString &title,
                               const QList<QWidget *> &rows) {
        if (QWidget *box = section(title, rows, content)) {
            m_layout->addWidget(box);
        }
    };
    add(tr("Since Yesterday"), yesterdayRows);
    add(tr("Now"), currentRows);
    add(tr("Upcoming"), upcomingRows);
    add(tr("Missed"), missedRows);
    add(tr("Completed"), doneRows);
    m_layout->addStretch(1);
}

void TrackerTodayPage::makeUpQada() {
    QadaDialog dialog(m_tracker, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_tracker->addQada(dialog.prayer(), dialog.date(), dialog.note());
}

QWidget *TrackerTodayPage::entryRow(const TrackerEntry &entry, bool yesterday) {
    const int index = PrayerTracker::prayerIndex(entry.prayer);

    QString name = PrayerTracker::prayerName(index);
    if (yesterday) {
        name = tr("%1 — since yesterday").arg(name);
    }
    if (entry.qada) {
        name = tr("%1 — make-up").arg(name);
    }
    auto *nameLabel = new QLabel(name);
    QFont bold = nameLabel->font();
    bold.setBold(true);
    nameLabel->setFont(bold);

    QString description = PrayerTracker::prayerDescription(index);
    auto *descriptionLabel = new QLabel(description);
    Ui::styleCaption(descriptionLabel);

    auto *labels = new QWidget;
    auto *labelLayout = new QVBoxLayout(labels);
    labelLayout->setContentsMargins(0, 0, 0, 0);
    labelLayout->setSpacing(0);
    labelLayout->addWidget(nameLabel);
    labelLayout->addWidget(descriptionLabel);

    auto *timeLabel = new QLabel(timeText(entry.start));
    timeLabel->setFont(Ui::tabularFont(timeLabel->font(), 1.0));

    QString status = PrayerTracker::statusLabel(entry.status);
    if (entry.pending()) {
        const qint64 now = m_tracker->nowEpoch();
        status = now < entry.start ? tr("Not started") : tr("Now");
    }
    auto *statusLabel = new QLabel(status);
    statusLabel->setMinimumWidth(90);
    statusLabel->setForegroundRole(QPalette::PlaceholderText);

    QList<QWidget *> widgets{labels, timeLabel, statusLabel};
    if (entry.pending()) {
        const QString id = entry.id;
        auto *confirm = new QPushButton(tr("Confirm"));
        confirm->setObjectName(QStringLiteral("TrackerConfirmButton"));
        auto *skip = new QPushButton(tr("Skip"));
        skip->setObjectName(QStringLiteral("TrackerSkipButton"));
        const bool open = m_tracker->isConfirmable(entry);
        confirm->setEnabled(open);
        skip->setEnabled(open);
        if (!open) {
            const QString tip = tr("The prayer time has not started yet.");
            confirm->setToolTip(tip);
            skip->setToolTip(tip);
        }
        connect(confirm, &QPushButton::clicked, this,
                [this, id] { m_tracker->confirm(id); });
        connect(skip, &QPushButton::clicked, this,
                [this, id] { m_tracker->skip(id); });
        widgets << confirm << skip;
    }
    return plainRow(widgets, 0);
}

QWidget *TrackerTodayPage::unavailableRow(int index) {
    auto *name = new QLabel(
        QStringLiteral("%1 — %2")
            .arg(PrayerTracker::prayerName(index),
                 PrayerTracker::prayerDescription(index)));
    name->setForegroundRole(QPalette::PlaceholderText);
    auto *time = new QLabel(QStringLiteral("N/A"));
    time->setForegroundRole(QPalette::PlaceholderText);
    auto *status = new QLabel(tr("Unavailable"));
    status->setForegroundRole(QPalette::PlaceholderText);
    auto *row = plainRow({name, time, status}, 0);
    row->setToolTip(tr("Prayer times are not available at this latitude."));
    return row;
}

TrackerHistoryPage::TrackerHistoryPage(PrayerTracker *tracker, QWidget *parent)
    : QWidget(parent), m_tracker(tracker) {
    setObjectName(QStringLiteral("TrackerHistoryPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Ui::controlSpacing());

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(Ui::controlSpacing());
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("TrackerSearch"));
    m_search->setClearButtonEnabled(true);
    m_search->setPlaceholderText(tr("Search the journal"));
    toolbar->addWidget(m_search, 1);
    auto *exportButton = new QPushButton(tr("Export CSV…"), this);
    exportButton->setObjectName(QStringLiteral("TrackerExportButton"));
    toolbar->addWidget(exportButton);
    layout->addLayout(toolbar);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("TrackerHistoryTable"));
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({tr("Date"), tr("Prayer"), tr("Status"),
                                        tr("Time"), tr("Make-up"), tr("Note")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    Ui::compactTable(m_table);
    m_table->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        5, QHeaderView::Stretch);
    layout->addWidget(m_table, 1);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(Ui::controlSpacing());
    buttons->addStretch(1);
    m_edit = new QPushButton(tr("Edit…"), this);
    m_edit->setObjectName(QStringLiteral("TrackerEditButton"));
    m_delete = new QPushButton(tr("Delete…"), this);
    m_delete->setObjectName(QStringLiteral("TrackerDeleteButton"));
    m_edit->setEnabled(false);
    m_delete->setEnabled(false);
    buttons->addWidget(m_edit);
    buttons->addWidget(m_delete);
    layout->addLayout(buttons);

    connect(m_search, &QLineEdit::textChanged, this,
            &TrackerHistoryPage::refresh);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool selected = !selectedId().isEmpty();
        m_edit->setEnabled(selected);
        m_delete->setEnabled(selected);
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { editSelected(); });
    connect(m_edit, &QPushButton::clicked, this,
            &TrackerHistoryPage::editSelected);
    connect(m_delete, &QPushButton::clicked, this,
            &TrackerHistoryPage::deleteSelected);
    connect(exportButton, &QPushButton::clicked, this,
            &TrackerHistoryPage::exportCsv);
}

void TrackerHistoryPage::refresh() {
    const QList<TrackerEntry> entries = m_tracker->history(m_search->text());
    m_table->setRowCount(0);
    for (const TrackerEntry &entry : entries) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *date = new QTableWidgetItem(entry.date.toString(Qt::ISODate));
        date->setData(Qt::UserRole, entry.id);
        m_table->setItem(row, 0, date);
        m_table->setItem(row, 1, new QTableWidgetItem(
            PrayerTracker::prayerName(
                PrayerTracker::prayerIndex(entry.prayer))));
        m_table->setItem(row, 2, new QTableWidgetItem(
            PrayerTracker::statusLabel(entry.status)));
        QString time = entry.scheduled;
        const QDateTime scheduled =
            QDateTime::fromString(entry.scheduled, Qt::ISODate);
        if (scheduled.isValid()) {
            time = timeText(scheduled.toSecsSinceEpoch());
        }
        m_table->setItem(row, 3, new QTableWidgetItem(time));
        m_table->setItem(row, 4, new QTableWidgetItem(
            entry.qada ? tr("Yes") : QString()));
        m_table->setItem(row, 5, new QTableWidgetItem(entry.note));
    }
    m_edit->setEnabled(false);
    m_delete->setEnabled(false);
}

QString TrackerHistoryPage::selectedId() const {
    const QList<QTableWidgetItem *> selected = m_table->selectedItems();
    if (selected.isEmpty()) {
        return {};
    }
    QTableWidgetItem *first = m_table->item(selected.first()->row(), 0);
    return first ? first->data(Qt::UserRole).toString() : QString();
}

void TrackerHistoryPage::editSelected() {
    const QString id = selectedId();
    if (id.isEmpty()) {
        return;
    }
    const TrackerEntry entry = m_tracker->entry(id);
    if (entry.id.isEmpty()) {
        return;
    }
    EditEntryDialog dialog(entry, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_tracker->updateEntry(dialog.result());
        refresh();
    }
}

void TrackerHistoryPage::deleteSelected() {
    const QString id = selectedId();
    if (id.isEmpty()) {
        return;
    }
    const TrackerEntry entry = m_tracker->entry(id);
    if (entry.id.isEmpty()) {
        return;
    }
    const QString question =
        tr("Delete the record for %1 on %2? The qada counter will be "
           "recalculated.")
            .arg(PrayerTracker::prayerName(
                     PrayerTracker::prayerIndex(entry.prayer)),
                 entry.date.toString(Qt::ISODate));
    if (QMessageBox::question(this, tr("Delete Journal Record"), question,
                              QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
        m_tracker->removeEntry(id);
        refresh();
    }
}

void TrackerHistoryPage::exportCsv() {
    const QString suggested =
        QStringLiteral("namaz-journal-")
        + QDate::currentDate().toString(Qt::ISODate) + QStringLiteral(".csv");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Journal"), suggested,
        tr("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    if (!m_tracker->exportCsv(path)) {
        QMessageBox::warning(this, tr("Export Journal"),
                             tr("Could not write %1.").arg(path));
    }
}

TrackerView::TrackerView(PrayerTracker *tracker, QWidget *parent)
    : QWidget(parent), m_tracker(tracker) {
    setObjectName(QStringLiteral("TrackerView"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(Ui::controlSpacing());

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("TrackerTabs"));
    m_tabs->setDocumentMode(true);
    m_today = new TrackerTodayPage(tracker, m_tabs);
    m_history = new TrackerHistoryPage(tracker, m_tabs);
    m_tabs->addTab(m_today, tr("Today"));
    m_tabs->addTab(m_history, tr("History"));
    layout->addWidget(m_tabs);

    connect(m_tabs, &QTabWidget::currentChanged, this,
            [this](int) { refreshCurrent(); });
    connect(m_tracker, &PrayerTracker::changed, this,
            &TrackerView::refresh);
    refresh();
}

void TrackerView::refreshCurrent() {
    if (!m_tabs) {
        return;
    }
    switch (m_tabs->currentIndex()) {
    case 0:
        m_today->refresh();
        break;
    default:
        m_history->refresh();
        break;
    }
}

void TrackerView::refresh() {
    refreshCurrent();
}
