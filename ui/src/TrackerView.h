#pragma once

#include "Tracker.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QTableWidget;
class QTabWidget;
class QTimer;
class QVBoxLayout;

class TrackerTodayPage : public QWidget {
    Q_OBJECT
public:
    explicit TrackerTodayPage(PrayerTracker *tracker, QWidget *parent = nullptr);
    void refresh();
    // Rebuilds the page only when a prayer window has started or ended since
    // the last refresh.  Called once per second while the page is visible.
    void refreshIfNeeded();

private:
    QWidget *entryRow(const TrackerEntry &entry, bool yesterday, qint64 now);
    QWidget *unavailableRow(int index);
    void clearContent();
    void makeUpQada();
    QString stateSignature(qint64 now) const;

    PrayerTracker *m_tracker;
    QVBoxLayout *m_layout = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_makeUp = nullptr;
    QString m_signature;
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

protected:
    void showEvent(QShowEvent *event) override;

private:
    void refreshCurrent();
    void refreshIfNeeded();

    PrayerTracker *m_tracker;
    QTabWidget *m_tabs = nullptr;
    TrackerTodayPage *m_today = nullptr;
    TrackerHistoryPage *m_history = nullptr;
    QTimer *m_refreshTimer = nullptr;
};
