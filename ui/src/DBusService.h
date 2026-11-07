#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>

class DBusService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.ikhlasulov_gb.Namaz")
    Q_PROPERTY(QString NextPrayerName READ nextPrayerName NOTIFY changed)
    Q_PROPERTY(QString NextPrayerTime READ nextPrayerTime NOTIFY changed)
    Q_PROPERTY(QString TimeRemaining READ timeRemaining NOTIFY changed)
    Q_PROPERTY(int TimeRemainingSeconds READ timeRemainingSeconds NOTIFY changed)
    Q_PROPERTY(QString Fajr READ fajr NOTIFY changed)
    Q_PROPERTY(QString Shuruq READ shuruq NOTIFY changed)
    Q_PROPERTY(QString Dhuhr READ dhuhr NOTIFY changed)
    Q_PROPERTY(QString Asr READ asr NOTIFY changed)
    Q_PROPERTY(QString Maghrib READ maghrib NOTIFY changed)
    Q_PROPERTY(QString Ishaa READ ishaa NOTIFY changed)
    Q_PROPERTY(QString Qiblah READ qiblah NOTIFY changed)
    Q_PROPERTY(QString Location READ location NOTIFY changed)
    Q_PROPERTY(QString CurrentDate READ currentDate NOTIFY changed)
    Q_PROPERTY(QString TopicItemList READ topicItemList NOTIFY changed)

public:
    explicit DBusService(QObject *parent = nullptr);

    bool enable();
    void disable();
    void update(const QJsonObject &day, int intervalSeconds);

    QString nextPrayerName() const { return m_nextPrayerName; }
    QString nextPrayerTime() const { return m_nextPrayerTime; }
    QString timeRemaining() const { return m_timeRemaining; }
    int timeRemainingSeconds() const { return m_timeRemainingSeconds; }
    QString fajr() const { return m_times[0]; }
    QString shuruq() const { return m_times[1]; }
    QString dhuhr() const { return m_times[2]; }
    QString asr() const { return m_times[3]; }
    QString maghrib() const { return m_times[4]; }
    QString ishaa() const { return m_times[5]; }
    QString qiblah() const { return m_qiblah; }
    QString location() const { return m_location; }
    QString currentDate() const { return m_currentDate; }
    QString topicItemList() const;

    bool isEnabled() const { return m_enabled; }

public slots:
    void Show();
    void Quit();
    QString GetTopicItemList() const;

signals:
    void changed();
    void showRequested();
    void quitRequested();

private:
    bool m_enabled = false;
    QString m_nextPrayerName;
    QString m_nextPrayerTime;
    QString m_timeRemaining;
    int m_timeRemainingSeconds = 0;
    QString m_times[6];
    QString m_qiblah;
    QString m_location;
    QString m_currentDate;
    QString m_lastKey;
    QDateTime m_lastUpdate;
};
