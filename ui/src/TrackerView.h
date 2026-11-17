#pragma once

#include "Tracker.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
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
    void makeUpQada();

    PrayerTracker *m_tracker;
    QVBoxLayout *m_layout = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_makeUp = nullptr;
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

class TrackerView : public QWidget {
    Q_OBJECT
public:
    explicit TrackerView(PrayerTracker *tracker, QWidget *parent = nullptr);

    void refresh();

private:
    void refreshCurrent();

    PrayerTracker *m_tracker;
    QTabWidget *m_tabs = nullptr;
    TrackerTodayPage *m_today = nullptr;
    TrackerHistoryPage *m_history = nullptr;
};
