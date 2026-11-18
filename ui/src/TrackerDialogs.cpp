#include "TrackerDialogs.h"

#include "UiUtils.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

void addStatusItem(QComboBox *combo, const QString &status) {
    combo->addItem(PrayerTracker::statusLabel(status), status);
}

}  // namespace

QadaDialog::QadaDialog(PrayerTracker *tracker, QWidget *parent)
    : QDialog(parent), m_tracker(tracker) {
    setWindowTitle(tr("Make Up a Missed Prayer"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(Ui::groupSpacing());

    auto *form = new QFormLayout();
    Ui::compactForm(form);

    m_prayer = new QComboBox(this);
    m_prayer->setObjectName(QStringLiteral("QadaPrayerCombo"));
    for (int index = 0; index < PrayerIndex::Count; ++index) {
        if (m_tracker && !m_tracker->tracked(index)) {
            continue;
        }
        m_prayer->addItem(PrayerTracker::prayerName(index),
                          PrayerTracker::prayerKey(index));
    }
    form->addRow(tr("Prayer"), m_prayer);

    m_date = new QDateEdit(QDate::currentDate(), this);
    m_date->setObjectName(QStringLiteral("QadaDateEdit"));
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    form->addRow(tr("Date"), m_date);

    m_note = new QLineEdit(this);
    m_note->setObjectName(QStringLiteral("QadaNoteEdit"));
    m_note->setPlaceholderText(tr("Optional note"));
    form->addRow(tr("Note"), m_note);

    m_debt = new QLabel(this);
    m_debt->setObjectName(QStringLiteral("QadaDebtLabel"));
    m_debt->setForegroundRole(QPalette::PlaceholderText);
    form->addRow(tr("Current debt"), m_debt);

    layout->addLayout(form);

    auto *hint = new QLabel(
        tr("A make-up prayer reduces the qada counter for the selected "
           "prayer."),
        this);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("DialogButtons"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_prayer, &QComboBox::currentIndexChanged, this,
            &QadaDialog::updateDebt);
    updateDebt();
}

QString QadaDialog::prayer() const {
    return m_prayer->currentData().toString();
}

QDate QadaDialog::date() const {
    return m_date->date();
}

QString QadaDialog::note() const {
    return m_note->text().trimmed();
}

void QadaDialog::updateDebt() {
    const int index = PrayerTracker::prayerIndex(prayer());
    if (m_tracker && index >= 0) {
        m_debt->setText(QString::number(m_tracker->qadaDebt(index)));
    }
}

EditEntryDialog::EditEntryDialog(const TrackerEntry &entry, QWidget *parent)
    : QDialog(parent), m_entry(entry) {
    setWindowTitle(tr("Edit Journal Record"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(Ui::groupSpacing());

    auto *form = new QFormLayout();
    Ui::compactForm(form);
    form->addRow(tr("Prayer"),
                 new QLabel(PrayerTracker::prayerName(
                                PrayerTracker::prayerIndex(entry.prayer)),
                            this));
    form->addRow(tr("Date"), new QLabel(entry.date.toString(Qt::ISODate), this));
    const qint64 moment = PrayerTracker::displayEpoch(entry);
    const QString scheduled =
        moment > 0
            ? QDateTime::fromSecsSinceEpoch(moment).toString(Qt::ISODate)
            : entry.scheduled;
    form->addRow(tr("Scheduled"), new QLabel(scheduled, this));

    m_status = new QComboBox(this);
    m_status->setObjectName(QStringLiteral("EditStatusCombo"));
    addStatusItem(m_status, QStringLiteral("confirmed"));
    addStatusItem(m_status, QStringLiteral("auto"));
    addStatusItem(m_status, QStringLiteral("skipped"));
    addStatusItem(m_status, QStringLiteral("missed"));
    m_status->setCurrentIndex(
        qMax(0, m_status->findData(entry.status)));
    form->addRow(tr("Status"), m_status);

    m_note = new QLineEdit(entry.note, this);
    m_note->setObjectName(QStringLiteral("EditNoteEdit"));
    form->addRow(tr("Note"), m_note);
    layout->addLayout(form);

    if (entry.qada) {
        auto *hint = new QLabel(
            tr("This record is a make-up prayer and counts against the qada "
               "debt."),
            this);
        hint->setWordWrap(true);
        hint->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(hint);
    }

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("DialogButtons"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

TrackerEntry EditEntryDialog::result() const {
    TrackerEntry entry = m_entry;
    entry.status = m_status->currentData().toString();
    entry.note = m_note->text().trimmed();
    return entry;
}
