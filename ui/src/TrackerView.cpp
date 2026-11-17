#include "TrackerView.h"

#include "TrackerDialogs.h"
#include "UiUtils.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDateTime>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
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

QIcon colorIcon(const QColor &color) {
    QPixmap pixmap(14, 14);
    pixmap.fill(color);
    return QIcon(pixmap);
}

QFrame *colorBar(const QColor &color, QWidget *parent) {
    auto *bar = new QFrame(parent);
    bar->setFixedWidth(4);
    bar->setAutoFillBackground(true);
    QPalette palette = bar->palette();
    palette.setColor(QPalette::Window, color);
    bar->setPalette(palette);
    return bar;
}

void clearLayout(QVBoxLayout *layout) {
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

QWidget *section(const QString &title, const QList<QWidget *> &rows,
                 QWidget *parent) {
    if (rows.isEmpty()) {
        return nullptr;
    }
    auto *box = new QGroupBox(title, parent);
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(Ui::controlSpacing());
    for (QWidget *row : rows) {
        layout->addWidget(row);
    }
    return box;
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

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    m_layout = new QVBoxLayout(content);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(Ui::groupSpacing());
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
        return;
    }

    const QDate today = QDateTime::fromSecsSinceEpoch(m_tracker->nowEpoch()).date();
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
                           : tr("Qada debt — %1").arg(debts.join(
                                 QStringLiteral("  ·  "))));

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

QWidget *TrackerTodayPage::entryRow(const TrackerEntry &entry, bool yesterday) {
    const int index = PrayerTracker::prayerIndex(entry.prayer);
    auto *row = new QFrame;
    row->setObjectName(QStringLiteral("TrackerRow_%1_%2")
                           .arg(entry.prayer, entry.date.toString(Qt::ISODate)));
    row->setFrameShape(QFrame::StyledPanel);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, Ui::controlSpacing(), 0);
    layout->setSpacing(Ui::controlSpacing());

    layout->addWidget(colorBar(QColor(m_tracker->prayerColor(index)), row));

    auto *icon = new QLabel(row);
    icon->setPixmap(Ui::themeIcon(m_tracker->prayerIcon(index),
                                  QStringLiteral("view-calendar"))
                        .pixmap(QSize(18, 18)));
    layout->addWidget(icon);

    auto *labels = new QWidget(row);
    auto *labelLayout = new QVBoxLayout(labels);
    labelLayout->setContentsMargins(0, 2, 0, 2);
    labelLayout->setSpacing(0);
    QString name = PrayerTracker::prayerName(index);
    if (yesterday) {
        name = tr("%1 — since yesterday").arg(name);
    }
    if (entry.qada) {
        name = tr("%1 — make-up").arg(name);
    }
    auto *nameLabel = new QLabel(name, labels);
    QFont bold = nameLabel->font();
    bold.setBold(true);
    nameLabel->setFont(bold);
    labelLayout->addWidget(nameLabel);
    QString description = PrayerTracker::prayerDescription(index);
    const QString notes = m_tracker->prayerNotes(index);
    if (!notes.isEmpty()) {
        description += QStringLiteral("  ·  ") + notes;
    }
    auto *descriptionLabel = new QLabel(description, labels);
    Ui::styleCaption(descriptionLabel);
    labelLayout->addWidget(descriptionLabel);
    layout->addWidget(labels, 1);

    auto *timeLabel = new QLabel(timeText(entry.start), row);
    timeLabel->setFont(Ui::tabularFont(timeLabel->font(), 1.0));
    layout->addWidget(timeLabel);

    QString status = PrayerTracker::statusLabel(entry.status);
    if (entry.pending()) {
        const qint64 now = m_tracker->nowEpoch();
        status = now < entry.start ? tr("Not started") : tr("Now");
    }
    auto *statusLabel = new QLabel(status, row);
    statusLabel->setMinimumWidth(90);
    if (entry.status == QLatin1String(TrackerStatus::Missed)) {
        QPalette palette = statusLabel->palette();
        palette.setColor(QPalette::WindowText, palette.color(QPalette::BrightText));
        statusLabel->setPalette(palette);
    } else if (entry.done()) {
        QPalette palette = statusLabel->palette();
        palette.setColor(QPalette::WindowText, palette.color(QPalette::Link));
        statusLabel->setPalette(palette);
    } else {
        statusLabel->setForegroundRole(QPalette::PlaceholderText);
    }
    layout->addWidget(statusLabel);

    if (entry.pending()) {
        const QString id = entry.id;
        auto *confirm = new QPushButton(tr("Confirm"), row);
        confirm->setObjectName(QStringLiteral("TrackerConfirmButton"));
        auto *skip = new QPushButton(tr("Skip"), row);
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
        layout->addWidget(confirm);
        layout->addWidget(skip);
    } else {
        layout->addSpacing(Ui::controlSpacing());
    }
    return row;
}

QWidget *TrackerTodayPage::unavailableRow(int index) {
    auto *row = new QFrame;
    row->setObjectName(QStringLiteral("TrackerUnavailable_%1")
                           .arg(PrayerTracker::prayerKey(index)));
    row->setFrameShape(QFrame::StyledPanel);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(Ui::controlSpacing(), 2, Ui::controlSpacing(), 2);
    auto *name = new QLabel(
        QStringLiteral("%1 — %2")
            .arg(PrayerTracker::prayerName(index),
                 PrayerTracker::prayerDescription(index)),
        row);
    name->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(name, 1);
    auto *time = new QLabel(QStringLiteral("N/A"), row);
    time->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(time);
    auto *status = new QLabel(tr("Unavailable"), row);
    status->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(status);
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
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
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
        const QDateTime scheduled = QDateTime::fromString(entry.scheduled, Qt::ISODate);
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

TrackerPrayersPage::TrackerPrayersPage(PrayerTracker *tracker, QWidget *parent)
    : QWidget(parent), m_tracker(tracker) {
    setObjectName(QStringLiteral("TrackerPrayersPage"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Ui::groupSpacing());
    scroll->setWidget(content);
    outer->addWidget(scroll);

    for (int index = 0; index < PrayerIndex::Count; ++index) {
        const QString key = PrayerTracker::prayerKey(index);
        auto *card = new QGroupBox(PrayerTracker::prayerName(index), content);
        card->setObjectName(QStringLiteral("PrayerCard_%1").arg(key));
        auto *form = new QFormLayout(card);
        Ui::compactForm(form);

        auto *description = new QLabel(
            PrayerTracker::prayerDescription(index), card);
        description->setForegroundRole(QPalette::PlaceholderText);
        description->setWordWrap(true);
        form->addRow(description);

        m_enabled[index] = new QCheckBox(tr("Track this prayer"), card);
        m_enabled[index]->setObjectName(QStringLiteral("TrackerEnabled_%1").arg(key));
        m_enabled[index]->setChecked(m_tracker->prayerEnabled(index));
        form->addRow(m_enabled[index]);

        m_notify[index] = new QCheckBox(tr("Show a reminder"), card);
        m_notify[index]->setObjectName(QStringLiteral("TrackerNotify_%1").arg(key));
        m_notify[index]->setChecked(m_tracker->notifyEnabled(index));
        form->addRow(m_notify[index]);

        m_auto[index] = new QCheckBox(
            tr("Mark as auto-confirmed after the window"), card);
        m_auto[index]->setObjectName(QStringLiteral("TrackerAuto_%1").arg(key));
        m_auto[index]->setChecked(m_tracker->autoConfirm(index));
        form->addRow(m_auto[index]);

        m_color[index] = new QPushButton(card);
        m_color[index]->setObjectName(QStringLiteral("TrackerColor_%1").arg(key));
        m_color[index]->setIcon(colorIcon(QColor(m_tracker->prayerColor(index))));
        m_color[index]->setText(m_tracker->prayerColor(index));
        form->addRow(tr("Colour"), m_color[index]);

        m_icon[index] = new QLineEdit(m_tracker->prayerIcon(index), card);
        m_icon[index]->setObjectName(QStringLiteral("TrackerIcon_%1").arg(key));
        m_icon[index]->setClearButtonEnabled(true);
        form->addRow(tr("Icon"), m_icon[index]);

        m_notes[index] = new QLineEdit(m_tracker->prayerNotes(index), card);
        m_notes[index]->setObjectName(QStringLiteral("TrackerNotes_%1").arg(key));
        m_notes[index]->setClearButtonEnabled(true);
        form->addRow(tr("Notes"), m_notes[index]);

        m_initialQada[index] = new QSpinBox(card);
        m_initialQada[index]->setObjectName(
            QStringLiteral("TrackerInitialQada_%1").arg(key));
        m_initialQada[index]->setRange(0, 1000000);
        m_initialQada[index]->setValue(
            static_cast<int>(m_tracker->initialQada(index)));
        form->addRow(tr("Initial qada"), m_initialQada[index]);

        m_debt[index] = new QLabel(card);
        m_debt[index]->setObjectName(
            QStringLiteral("TrackerQadaDebt_%1").arg(key));
        form->addRow(tr("Debt"), m_debt[index]);

        auto *makeUpButton = new QPushButton(tr("Make up a missed prayer…"), card);
        makeUpButton->setObjectName(QStringLiteral("TrackerQadaButton_%1").arg(key));
        form->addRow(makeUpButton);

        connect(m_enabled[index], &QCheckBox::toggled, this,
                [this, index](bool on) {
                    updateSetting(QStringLiteral("enabled_prayers"), index, on);
                });
        connect(m_notify[index], &QCheckBox::toggled, this,
                [this, index](bool on) {
                    updateSetting(QStringLiteral("notify"), index, on);
                });
        connect(m_auto[index], &QCheckBox::toggled, this,
                [this, index](bool on) {
                    updateSetting(QStringLiteral("auto_confirm"), index, on);
                });
        connect(m_color[index], &QPushButton::clicked, this,
                [this, index] { chooseColor(index); });
        connect(m_icon[index], &QLineEdit::editingFinished, this, [this, index] {
            updateSetting(QStringLiteral("icons"), index, m_icon[index]->text());
        });
        connect(m_notes[index], &QLineEdit::editingFinished, this, [this, index] {
            updateSetting(QStringLiteral("notes"), index, m_notes[index]->text());
        });
        connect(m_initialQada[index], &QSpinBox::valueChanged, this,
                [this, index](int value) {
                    m_tracker->setInitialQada(index, value);
                });
        connect(makeUpButton, &QPushButton::clicked, this,
                [this, index] { makeUp(index); });

        layout->addWidget(card);
    }
    layout->addStretch(1);
    refresh();
}

void TrackerPrayersPage::updateSetting(const QString &group, int index,
                                       const QJsonValue &value) {
    QJsonObject settings = m_tracker->settings();
    QJsonObject map = settings.value(group).toObject();
    map.insert(PrayerTracker::prayerKey(index), value);
    settings.insert(group, map);
    m_tracker->setSettings(settings);
}

void TrackerPrayersPage::chooseColor(int index) {
    const QColor color = QColorDialog::getColor(
        QColor(m_tracker->prayerColor(index)), this, tr("Prayer Colour"));
    if (!color.isValid()) {
        return;
    }
    updateSetting(QStringLiteral("colors"), index, color.name());
    m_color[index]->setIcon(colorIcon(color));
    m_color[index]->setText(color.name());
}

void TrackerPrayersPage::makeUp(int index) {
    QadaDialog dialog(m_tracker, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_tracker->addQada(dialog.prayer(), dialog.date(), dialog.note());
}

void TrackerPrayersPage::refresh() {
    for (int index = 0; index < PrayerIndex::Count; ++index) {
        if (m_debt[index]) {
            m_debt[index]->setText(
                QString::number(m_tracker->qadaDebt(index)));
        }
        if (m_initialQada[index]) {
            const QSignalBlocker blocker(m_initialQada[index]);
            m_initialQada[index]->setValue(
                static_cast<int>(m_tracker->initialQada(index)));
        }
        if (m_enabled[index]) {
            const QSignalBlocker blocker(m_enabled[index]);
            m_enabled[index]->setChecked(m_tracker->prayerEnabled(index));
        }
        if (m_notify[index]) {
            const QSignalBlocker blocker(m_notify[index]);
            m_notify[index]->setChecked(m_tracker->notifyEnabled(index));
        }
        if (m_auto[index]) {
            const QSignalBlocker blocker(m_auto[index]);
            m_auto[index]->setChecked(m_tracker->autoConfirm(index));
        }
        if (m_color[index]) {
            m_color[index]->setIcon(
                colorIcon(QColor(m_tracker->prayerColor(index))));
            m_color[index]->setText(m_tracker->prayerColor(index));
        }
    }
}

TrackerView::TrackerView(PrayerTracker *tracker, QWidget *parent)
    : QWidget(parent), m_tracker(tracker) {
    setObjectName(QStringLiteral("TrackerView"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("TrackerTabs"));
    m_tabs->setDocumentMode(true);
    m_today = new TrackerTodayPage(tracker, m_tabs);
    m_history = new TrackerHistoryPage(tracker, m_tabs);
    m_prayers = new TrackerPrayersPage(tracker, m_tabs);
    m_tabs->addTab(m_today, tr("Today"));
    m_tabs->addTab(m_history, tr("History"));
    m_tabs->addTab(m_prayers, tr("Prayers"));
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
    case 1:
        m_history->refresh();
        break;
    default:
        m_prayers->refresh();
        break;
    }
}

void TrackerView::refresh() {
    refreshCurrent();
}
