#pragma once

#include "Tracker.h"

#include <QJsonValue>
#include <QWidget>

class QLabel;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QVBoxLayout;

class TrackerTodayPage : public QWidget {
    Q_OBJECT
public:
    explicit TrackerTodayPage(PrayerTracker *tracker, QWidget *parent = nullptr);
    void refresh();

private:
    QWidget *entryRow(const TrackerEntry &entry, bool yesterday);
    QWidget *unavailableRow(int index);
    void clearContent();

    PrayerTracker *m_tracker;
    QVBoxLayout *m_layout = nullptr;
    QLabel *m_summary = nullptr;
};

class TrackerHistoryPage : public QWidget {
    Q_OBJECT
public:
    explicit TrackerHistoryPage(PrayerTracker *tracker,
                                QWidget *parent = nullptr);
    void refresh();

private:
    void editSelected();
    void deleteSelected();
    void exportCsv();
    QString selectedId() const;

    PrayerTracker *m_tracker;
    QLineEdit *m_search = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_edit = nullptr;
    QPushButton *m_delete = nullptr;
};

class TrackerPrayersPage : public QWidget {
    Q_OBJECT
public:
    explicit TrackerPrayersPage(PrayerTracker *tracker,
                                QWidget *parent = nullptr);
    void refresh();

private:
    void updateSetting(const QString &group, int index,
                       const QJsonValue &value);
    void chooseColor(int index);
    void makeUp(int index);

    PrayerTracker *m_tracker;
    QCheckBox *m_enabled[PrayerIndex::Count] = {};
    QCheckBox *m_notify[PrayerIndex::Count] = {};
    QCheckBox *m_auto[PrayerIndex::Count] = {};
    QSpinBox *m_initialQada[PrayerIndex::Count] = {};
    QLabel *m_debt[PrayerIndex::Count] = {};
    QPushButton *m_color[PrayerIndex::Count] = {};
    QLineEdit *m_icon[PrayerIndex::Count] = {};
    QLineEdit *m_notes[PrayerIndex::Count] = {};
};

class TrackerView : public QWidget {
    Q_OBJECT
public:
    explicit TrackerView(PrayerTracker *tracker, QWidget *parent = nullptr);

    void refresh();
    QTabWidget *tabs() const { return m_tabs; }

private:
    void refreshCurrent();

    PrayerTracker *m_tracker;
    QTabWidget *m_tabs = nullptr;
    TrackerTodayPage *m_today = nullptr;
    TrackerHistoryPage *m_history = nullptr;
    TrackerPrayersPage *m_prayers = nullptr;
};
