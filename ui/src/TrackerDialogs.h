#pragma once

#include "Tracker.h"

#include <QDate>
#include <QDialog>
#include <QString>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;

// "Совершить када": records a make-up prayer with `qada=true`.
class QadaDialog : public QDialog {
    Q_OBJECT
public:
    explicit QadaDialog(PrayerTracker *tracker, QWidget *parent = nullptr);

    QString prayer() const;
    QDate date() const;
    QString note() const;

private:
    void updateDebt();

    PrayerTracker *m_tracker;
    QComboBox *m_prayer = nullptr;
    QDateEdit *m_date = nullptr;
    QLineEdit *m_note = nullptr;
    QLabel *m_debt = nullptr;
};

// Editing one history record.
class EditEntryDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditEntryDialog(const TrackerEntry &entry, QWidget *parent = nullptr);

    TrackerEntry result() const;

private:
    TrackerEntry m_entry;
    QComboBox *m_status = nullptr;
    QLineEdit *m_note = nullptr;
};
