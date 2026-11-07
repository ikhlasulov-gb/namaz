#pragma once

#include "Core.h"
#include "UpdateChecker.h"

#include <QDate>
#include <QJsonObject>
#include <QMainWindow>
#include <QWidget>

class QAction;
class QActionGroup;
class QCloseEvent;
class QHBoxLayout;
class QLabel;
class QResizeEvent;
class QStackedWidget;
class QShortcut;
class QSystemTrayIcon;
class QTimer;
class QToolBar;
class DayView;
class MonthView;
class YearView;
class MasjidWindow;
class DBusService;
class PrayerSettingsDialog;
class PreferencesDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void setTab(int index);
    void playStartupSound();
    void openSettingsDialog(int tab = 0);
    void openPreferencesDialog(int tab = 0);
    void exportCsvMonth(int month, int calendar = 0);
    void exportCsvYear(int calendar = 0);
    void showMasjid();
    bool printMonthToPath(const QString &path, int calendar = 0);

    bool isTrayAvailable() const;
    bool startMinimized() const;
    bool shouldShowSplash() const;
    void showFromTray();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildActions();
    void buildMenus();
    void buildToolBar();
    void buildTray();
    void buildShortcuts();
    void tick();
    void refreshFromCore();
    void refreshCalendar();
    void refreshAll();
    void shiftDay(int days);
    void shiftMonth(int months);
    void shiftYear(int years);
    void goToday();
    void setSelectedDate(const QDate &date);
    void applyPreferencesToWindow();
    void saveWindowState();
    void restoreWindowState();
    void updateTray();
    void flashTray();
    void playSound(const QString &name);
    void showAudioError(const QString &name);
    void printMonth(bool toPdf, int calendar = 0);
    void prayNow();
    void syncAutostart();
    void restartForLanguage();
    void checkForUpdates(bool manual);
    void showUpdateDialog(const UpdateChecker::Release &release);
    void installAppImageUpdate(const UpdateChecker::Release &release);
    QJsonObject config() const;

    Core m_core;
    QDate m_selectedDate;
    QString m_locale;

    QStackedWidget *m_views = nullptr;
    QHBoxLayout *m_centralLayout = nullptr;
    QToolBar *m_toolBar = nullptr;
    DayView *m_dayView = nullptr;
    MonthView *m_monthView = nullptr;
    YearView *m_yearView = nullptr;
    QLabel *m_cityLabel = nullptr;
    QLabel *m_methodLabel = nullptr;

    PrayerSettingsDialog *m_settingsDialog = nullptr;
    PreferencesDialog *m_preferencesDialog = nullptr;
    QList<MasjidWindow *> m_masjidWindows;
    DBusService *m_dbus = nullptr;
    UpdateChecker *m_updater = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QAction *m_trayNextAction = nullptr;
    QAction *m_muteAction = nullptr;
    QActionGroup *m_viewGroup = nullptr;
    QAction *m_viewActions[3] = {};
    QAction *m_previousAction = nullptr;
    QAction *m_todayAction = nullptr;
    QAction *m_nextAction = nullptr;
    QAction *m_qiblahAction = nullptr;
    QAction *m_hijriAction = nullptr;
    QAction *m_menuMuteAction = nullptr;
    QShortcut *m_dayLeftShortcut = nullptr;
    QShortcut *m_dayRightShortcut = nullptr;
    QTimer *m_flashTimer = nullptr;
    bool m_flashOn = false;
};
