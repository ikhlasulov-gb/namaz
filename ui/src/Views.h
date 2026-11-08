#pragma once

#include <QDateTime>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QBoxLayout;
class QCalendarWidget;
class QEvent;
class QLabel;
class QProgressBar;
class QTableWidget;
class QScreen;

// A palette-only circular compass: ring, cardinal ticks, N/E/S/W and a needle
// that points at the real Qiblah bearing (clockwise from true north).
class QiblahCompass : public QWidget {
    Q_OBJECT
public:
    explicit QiblahCompass(QWidget *parent = nullptr);

    void setBearing(qreal bearing);
    qreal bearing() const { return m_bearing; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_bearing = 0.0;
};

class CurrentTimeCard : public QGroupBox {
    Q_OBJECT
public:
    explicit CurrentTimeCard(QWidget *parent = nullptr);
    void setState(const QJsonObject &state);

private:
    QLabel *m_timeLabel = nullptr;
    QLabel *m_prayerLabel = nullptr;
};

class NextPrayerCard : public QGroupBox {
    Q_OBJECT
public:
    explicit NextPrayerCard(QWidget *parent = nullptr);
    void setState(const QJsonObject &state);

private:
    QLabel *m_timeLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_countdownLabel = nullptr;
    QProgressBar *m_progress = nullptr;
};

class PrayerTimesWidget : public QGroupBox {
    Q_OBJECT
public:
    explicit PrayerTimesWidget(QWidget *parent = nullptr);
    void setState(const QJsonObject &state);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyTablePalette();
    void updateColumnWidths();

    QTableWidget *m_table = nullptr;
};

class QiblahCard : public QGroupBox {
    Q_OBJECT
public:
    explicit QiblahCard(QWidget *parent = nullptr);
    void setState(const QJsonObject &qibla);

private:
    QiblahCompass *m_compass = nullptr;
    QLabel *m_angle = nullptr;
    QLabel *m_suffix = nullptr;
};

// Switches between a horizontal row and a vertical stack when the available
// width becomes too small for both children.
class ResponsiveRow : public QWidget {
    Q_OBJECT
public:
    ResponsiveRow(QWidget *first, QWidget *second, int breakpoint, int spacing,
                  int firstStretch, int secondStretch, QWidget *parent = nullptr,
                  int horizontalMaxHeight = QWIDGETSIZE_MAX);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateDirection();

    QBoxLayout *m_layout = nullptr;
    int m_breakpoint = 0;
    int m_firstStretch = 1;
    int m_secondStretch = 1;
    int m_horizontalMaxHeight = QWIDGETSIZE_MAX;
    bool m_stacked = false;
};

class DayView : public QWidget {
    Q_OBJECT
public:
    explicit DayView(QWidget *parent = nullptr);
    void setState(const QJsonObject &state);

private:
    QLabel *m_dateLabel = nullptr;
    QLabel *m_hijriLabel = nullptr;
    CurrentTimeCard *m_timeCard = nullptr;
    NextPrayerCard *m_nextCard = nullptr;
    PrayerTimesWidget *m_prayerTimes = nullptr;
    QiblahCard *m_qiblahCard = nullptr;
    ResponsiveRow *m_cards = nullptr;
    ResponsiveRow *m_content = nullptr;
};

class MonthView : public QWidget {
    Q_OBJECT
public:
    explicit MonthView(QWidget *parent = nullptr);
    void setData(const QJsonObject &month, const QJsonObject &day);

signals:
    void dateSelected(const QDate &date);

private:
    void rebuildHijri(const QJsonArray &cells, const QString &title);
    void populateTimes(const QJsonObject &day, const QString &selectedLabel);
    QCalendarWidget *m_calendar = nullptr;
    QLabel *m_hijriTitle = nullptr;
    QTableWidget *m_hijriTable = nullptr;
    QTableWidget *m_timesTable = nullptr;
    QWidget *m_gregorianGroup = nullptr;
    QWidget *m_hijriGroup = nullptr;
    QWidget *m_timesGroup = nullptr;
    QDate m_selectedDate;
    bool m_updating = false;
};

class YearView : public QWidget {
    Q_OBJECT
public:
    explicit YearView(QWidget *parent = nullptr);
    void setData(const QJsonObject &year);

signals:
    void openDayRequested(const QDate &date);

private:
    QLabel *m_title = nullptr;
    QTableWidget *m_table = nullptr;
};

class MasjidWindow : public QWidget {
    Q_OBJECT
public:
    explicit MasjidWindow(QWidget *parent = nullptr);
    void setState(const QJsonObject &state);
    void setScreen(QScreen *screen);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel *m_clock = nullptr;
    QLabel *m_date = nullptr;
    QLabel *m_next = nullptr;
    QLabel *m_iqama = nullptr;
    QLabel *m_names[6] = {};
    QLabel *m_times[6] = {};
    QProgressBar *m_progress = nullptr;
};
