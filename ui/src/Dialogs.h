#pragma once

#include <QDialog>
#include <QJsonObject>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QTimeEdit;
class QWidget;

class PrayerSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit PrayerSettingsDialog(void *core, QWidget *parent = nullptr);
    void setTabIndex(int index);
    void reload();

signals:
    void applied();

private:
    void populate(const QJsonObject &config);
    bool validate();
    bool apply();
    void loadCountries();
    void loadCities(const QString &country, bool apply);
    void selectCity();
    void updateMethodDescription();
    void updateExtremeDescription();
    void populateTimeZones();

    void *m_core;
    bool m_updating = false;
    bool m_populating = false;

    QListWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;

    QLineEdit *m_city = nullptr;
    QLineEdit *m_province = nullptr;
    QListWidget *m_countries = nullptr;
    QListWidget *m_cities = nullptr;
    QDoubleSpinBox *m_latitude = nullptr;
    QDoubleSpinBox *m_longitude = nullptr;
    QDoubleSpinBox *m_timezone = nullptr;
    QComboBox *m_timeZoneName = nullptr;
    QComboBox *m_dstMode = nullptr;
    QSpinBox *m_dstManual = nullptr;
    QComboBox *m_method = nullptr;
    QLabel *m_methodDescription = nullptr;
    QComboBox *m_mathhab = nullptr;
    QComboBox *m_rounding = nullptr;
    QComboBox *m_extreme = nullptr;
    QLabel *m_extremeDescription = nullptr;
    QDoubleSpinBox *m_fajrAngle = nullptr;
    QDoubleSpinBox *m_ishaaAngle = nullptr;
    QSpinBox *m_dhuhrMinutes = nullptr;
    QSpinBox *m_maghribMinutes = nullptr;
    QSpinBox *m_adjust[6] = {};
};

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(void *core, QWidget *parent = nullptr);
    void setTabIndex(int index);
    void reload();

signals:
    void applied();

private:
    void populate(const QJsonObject &config);
    bool apply();
    void restoreDefaults();
    void updateAudioRow(int index);
    void updateStartupRow();
    void updateReminderAudioRow();
    void updateMasjidScreens();
    void browseFile(QLineEdit *target);
    void browseFolder(QLineEdit *target);
    void previewSound(const QString &name);
    void previewFile(const QLineEdit *edit);
    void previewFolder(const QLineEdit *edit);

    void *m_core;

    QListWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;

    QCheckBox *m_athanEnabled = nullptr;
    QCheckBox *m_notifications = nullptr;
    QCheckBox *m_duaAfterAthan = nullptr;
    QCheckBox *m_stopOnPrayer = nullptr;
    QSlider *m_volume = nullptr;
    QLabel *m_volumeValue = nullptr;

    QComboBox *m_audioMode[6] = {};
    QComboBox *m_sounds[6] = {};
    QLineEdit *m_soundFile[6] = {};
    QLineEdit *m_soundFolder[6] = {};
    QStackedWidget *m_audioStack[6] = {};

    QComboBox *m_startupMode = nullptr;
    QComboBox *m_startupSound = nullptr;
    QLineEdit *m_startupFile = nullptr;
    QLineEdit *m_startupFolder = nullptr;
    QStackedWidget *m_startupStack = nullptr;

    QCheckBox *m_beforeOn[6] = {};
    QSpinBox *m_beforeMinutes[6] = {};
    QCheckBox *m_afterOn[6] = {};
    QSpinBox *m_afterMinutes[6] = {};
    QCheckBox *m_fixedOn[6] = {};
    QTimeEdit *m_fixedTime[6] = {};
    QCheckBox *m_iqamaOn[6] = {};
    QSpinBox *m_iqamaMinutes[6] = {};
    QCheckBox *m_reminderNotification = nullptr;
    QCheckBox *m_reminderMain = nullptr;
    QCheckBox *m_reminderFlash = nullptr;
    QComboBox *m_reminderAudioMode = nullptr;
    QLineEdit *m_reminderFile = nullptr;
    QLineEdit *m_reminderFolder = nullptr;
    QWidget *m_reminderFileRow = nullptr;
    QWidget *m_reminderFolderRow = nullptr;
    QWidget *m_reminderFileLabel = nullptr;
    QWidget *m_reminderFolderLabel = nullptr;

    QCheckBox *m_alwaysOnTop = nullptr;
    QCheckBox *m_hideOnClose = nullptr;
    QCheckBox *m_launchMinimized = nullptr;
    QCheckBox *m_launchOnStartup = nullptr;
    QCheckBox *m_athanVisualMain = nullptr;
    QCheckBox *m_athanVisualFlash = nullptr;
    QCheckBox *m_hijriCalendar = nullptr;
    QCheckBox *m_showQiblah = nullptr;
    QCheckBox *m_qiblaClockwise = nullptr;
    QCheckBox *m_lockOnPray = nullptr;
    QCheckBox *m_screensaverOnPray = nullptr;
    QCheckBox *m_showSplash = nullptr;
    QCheckBox *m_dbusEnabled = nullptr;
    QSpinBox *m_dbusInterval = nullptr;
    QSpinBox *m_hijriOffset = nullptr;
    QComboBox *m_masjidScreen = nullptr;
    QComboBox *m_language = nullptr;
};
