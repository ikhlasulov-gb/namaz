#include "Widgets.h"

#include "DBusService.h"
#include "Dialogs.h"
#include "UiUtils.h"
#include "Views.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPrintDialog>
#include <QPrinter>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTemporaryFile>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Keep the default window comfortable on 1280x800/150 % screens as well.
QSize initialWindowSize() {
    QSize available(1920, 1200);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        available = screen->availableGeometry().size();
    }
    return QSize(qMin(920, qMax(640, available.width() * 9 / 10)),
                 qMin(660, qMax(480, available.height() * 9 / 10)));
}

QString htmlForCsv(const QString &csv, const QString &title,
                   const QString &subtitle) {
    const QStringList lines = csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QString html = QStringLiteral(
        "<html><head><style>"
        "body { font-family: sans-serif; }"
        "h1 { font-size: 16pt; margin-bottom: 2px; }"
        "p { font-size: 9pt; margin-top: 0; }"
        "table { border-collapse: collapse; width: 100%; }"
        "th, td { border: 1px solid #808080; padding: 3px 6px; "
        "font-size: 9pt; text-align: center; }"
        "th { font-weight: bold; }"
        "</style></head><body>");
    html += QStringLiteral("<h1>%1</h1><p>%2</p><table>")
                .arg(title.toHtmlEscaped(), subtitle.toHtmlEscaped());
    for (int index = 0; index < lines.size(); ++index) {
        const QStringList cells = lines.at(index).split(QLatin1Char(','));
        html += QStringLiteral("<tr>");
        for (const QString &cell : cells) {
            html += index == 0 ? QStringLiteral("<th>%1</th>").arg(cell.toHtmlEscaped())
                               : QStringLiteral("<td>%1</td>").arg(cell.toHtmlEscaped());
        }
        html += QStringLiteral("</tr>");
    }
    html += QStringLiteral("</table></body></html>");
    return html;
}

}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Namaz"));
    QIcon icon = Ui::appIcon();
    setWindowIcon(icon);
    setMinimumSize(640, 480);
    resize(initialWindowSize());

    buildActions();
    buildMenus();
    buildToolBar();

    m_views = new QStackedWidget(this);
    m_views->setObjectName(QStringLiteral("ViewStack"));

    m_dayView = new DayView(m_views);
    m_monthView = new MonthView(m_views);
    m_yearView = new YearView(m_views);

    m_views->addWidget(m_dayView);
    m_views->addWidget(m_monthView);
    m_views->addWidget(m_yearView);

    // Keep the content from stretching across very wide screens.
    auto *central = new QWidget(this);
    m_centralLayout = new QHBoxLayout(central);
    m_centralLayout->setContentsMargins(0, 0, 0, 0);
    m_centralLayout->setSpacing(0);
    m_centralLayout->addWidget(m_views);
    setCentralWidget(central);

    restoreWindowState();

    connect(m_monthView, &MonthView::dateSelected, this, &MainWindow::setSelectedDate);
    connect(m_yearView, &YearView::openDayRequested, this, [this](const QDate &date) {
        setSelectedDate(date);
        setTab(0);
    });

    m_cityLabel = new QLabel(this);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_cityLabel, 1);
    m_methodLabel = new QLabel(this);
    m_methodLabel->setForegroundRole(QPalette::PlaceholderText);
    statusBar()->addPermanentWidget(m_methodLabel);

    connect(m_views, &QStackedWidget::currentChanged, this, [this](int index) {
        namaz_core_set_view(m_core.handle(), index);
        // QActionGroup keeps exactly one action checked; blocking the signal
        // here would leave the previous action checked as well.
        if (index >= 0 && index < 3 && m_viewActions[index]) {
            m_viewActions[index]->setChecked(true);
        }
        if (m_dayLeftShortcut) {
            m_dayLeftShortcut->setEnabled(index == 0);
        }
        if (m_dayRightShortcut) {
            m_dayRightShortcut->setEnabled(index == 0);
        }
        refreshCalendar();
    });

    buildShortcuts();
    buildTray();

    m_dbus = new DBusService(this);
    connect(m_dbus, &DBusService::showRequested, this, &MainWindow::showFromTray);
    connect(m_dbus, &DBusService::quitRequested, qApp, &QApplication::quit);
    connect(qApp, &QCoreApplication::aboutToQuit, m_dbus, &DBusService::disable);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveWindowState);

    m_updater = new UpdateChecker(this);
    connect(m_updater, &UpdateChecker::updateAvailable, this,
            [this](const UpdateChecker::Release &release) {
                showUpdateDialog(release);
            });
    connect(m_updater, &UpdateChecker::upToDate, this, [this](const QString &version) {
        QMessageBox::information(
            this, tr("Check for Updates"),
            tr("Namaz %1 is the latest available version.").arg(version));
    });
    connect(m_updater, &UpdateChecker::failed, this, [this](const QString &message) {
        QMessageBox::warning(
            this, tr("Check for Updates"),
            tr("Could not check for updates:\n%1").arg(message));
    });
    // Automatic check is silent and runs at most once per day.
    QTimer::singleShot(15000, this, [this] { checkForUpdates(false); });

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::tick);
    timer->start(1000);

    m_locale = config().value(QStringLiteral("locale")).toString();
    if (m_locale.isEmpty()) {
        m_locale = QStringLiteral("system");
    }
    applyPreferencesToWindow();
    namaz_core_set_volume(config().value(QStringLiteral("volume")).toInt(90));
    refreshAll();
}

void MainWindow::buildActions() {
    m_viewGroup = new QActionGroup(this);
    m_viewGroup->setExclusive(true);

    const struct {
        const char *label;
        const char *icon;
        const char *shortcut;
    } views[] = {
        {QT_TR_NOOP("&Day"), "view-calendar-day", "Ctrl+1"},
        {QT_TR_NOOP("&Month"), "view-calendar-month", "Ctrl+2"},
        {QT_TR_NOOP("&Year"), "view-calendar-year", "Ctrl+3"},
    };
    for (int index = 0; index < 3; ++index) {
        QAction *action = new QAction(Ui::themeIcon(QString::fromLatin1(views[index].icon),
                                                    QStringLiteral("view-calendar")),
                                      tr(views[index].label), this);
        action->setCheckable(true);
        action->setChecked(index == 0);
        action->setShortcut(QKeySequence(QString::fromLatin1(views[index].shortcut)));
        m_viewGroup->addAction(action);
        m_viewActions[index] = action;
        connect(action, &QAction::triggered, this, [this, index] { setTab(index); });
    }

    m_previousAction = new QAction(Ui::themeIcon(QStringLiteral("go-previous")),
                                   tr("Previous"), this);
    m_previousAction->setShortcut(QKeySequence(QStringLiteral("Alt+Left")));
    connect(m_previousAction, &QAction::triggered, this, [this] { shiftDay(-1); });

    m_todayAction = new QAction(Ui::themeIcon(QStringLiteral("go-home")),
                                tr("Today"), this);
    m_todayAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    connect(m_todayAction, &QAction::triggered, this, &MainWindow::goToday);

    m_nextAction = new QAction(Ui::themeIcon(QStringLiteral("go-next")),
                               tr("Next"), this);
    m_nextAction->setShortcut(QKeySequence(QStringLiteral("Alt+Right")));
    connect(m_nextAction, &QAction::triggered, this, [this] { shiftDay(1); });
}

void MainWindow::buildToolBar() {
    m_toolBar = addToolBar(tr("Main Toolbar"));
    m_toolBar->setObjectName(QStringLiteral("MainToolBar"));
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setIconSize(QSize(16, 16));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toolBar->setContextMenuPolicy(Qt::PreventContextMenu);

    m_toolBar->addAction(m_viewActions[0]);
    m_toolBar->addAction(m_viewActions[1]);
    m_toolBar->addAction(m_viewActions[2]);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_previousAction);
    m_toolBar->addAction(m_todayAction);
    m_toolBar->addAction(m_nextAction);
    m_toolBar->addSeparator();

    QAction *pray = m_toolBar->addAction(
        Ui::themeIcon(QStringLiteral("media-playback-start")), tr("Pray Now"));
    connect(pray, &QAction::triggered, this, &MainWindow::prayNow);
    QAction *settings = m_toolBar->addAction(
        Ui::themeIcon(QStringLiteral("preferences-system-time")), tr("Settings"));
    connect(settings, &QAction::triggered, this, [this] { openSettingsDialog(); });

    // Breeze 6.6 does not paint the text of auto-raised (flat) tool buttons
    // unless they are checked, which makes the toolbar unreadable in the dark
    // color scheme.  Explicitly disable auto-raise on Breeze so every label
    // stays visible; other styles keep their native flat tool buttons.
    if (QApplication::style()->name().compare(QLatin1String("breeze"),
                                               Qt::CaseInsensitive)
        == 0) {
        for (QAction *action : m_toolBar->actions()) {
            if (auto *button = qobject_cast<QToolButton *>(
                    m_toolBar->widgetForAction(action))) {
                button->setAutoRaise(false);
            }
        }
    }
}

void MainWindow::buildMenus() {
    auto addSound = [this](QMenu *menu, const QString &label, const char *name) {
        QAction *action = menu->addAction(label);
        connect(action, &QAction::triggered, this, [this, name] {
            playSound(QString::fromLatin1(name));
        });
    };

    QMenu *file = menuBar()->addMenu(tr("&File"));

    QMenu *exportMenu = file->addMenu(tr("&Export Prayer Times"));
    const QString monthNames[] = {
        tr("January"), tr("February"), tr("March"), tr("April"),
        tr("May"), tr("June"), tr("July"), tr("August"),
        tr("September"), tr("October"), tr("November"), tr("December")};
    auto addExportCalendar = [&](QMenu *parentMenu, const QString &title, int calendar,
                                 bool hijri) {
        QMenu *calendarMenu = parentMenu->addMenu(title);
        for (int index = 0; index < 12; ++index) {
            const QString label = hijri ? Ui::hijriMonthName(index + 1)
                                        : monthNames[index];
            QAction *action = calendarMenu->addAction(label);
            connect(action, &QAction::triggered, this, [this, index, calendar] {
                // For the Hijri menu, index+1 is a Hijri month number.
                exportCsvMonth(index + 1, calendar);
            });
        }
        calendarMenu->addSeparator();
        QAction *yearAction = calendarMenu->addAction(tr("Whole Year"));
        connect(yearAction, &QAction::triggered, this, [this, calendar] {
            exportCsvYear(calendar);
        });
    };
    addExportCalendar(exportMenu, tr("Gregorian Calendar"), 0, false);
    addExportCalendar(exportMenu, tr("Hijri Calendar"), 1, true);
    addExportCalendar(exportMenu, tr("Outlook CSV"), 2, false);

    QMenu *printMenu = file->addMenu(tr("&Print Prayer Times"));
    auto addPrint = [this](QMenu *menu, const QString &title, int calendar,
                           bool pdf, const QKeySequence &shortcut = QKeySequence()) {
        QAction *action = menu->addAction(title);
        if (!shortcut.isEmpty()) {
            action->setShortcut(shortcut);
        }
        connect(action, &QAction::triggered, this, [this, calendar, pdf] {
            printMonth(pdf, calendar);
        });
    };
    addPrint(printMenu, tr("Gregorian month"), 0, false, QKeySequence::Print);
    addPrint(printMenu, tr("Hijri month"), 1, false);
    QMenu *pdfMenu = file->addMenu(tr("Print to &PDF"));
    addPrint(pdfMenu, tr("Gregorian month"), 0, true);
    addPrint(pdfMenu, tr("Hijri month"), 1, true);

    file->addSeparator();
    QAction *quit = file->addAction(tr("&Quit"));
    quit->setShortcut(QKeySequence::Quit);
    quit->setMenuRole(QAction::QuitRole);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    for (int index = 0; index < 3; ++index) {
        view->addAction(m_viewActions[index]);
    }
    view->addSeparator();
    view->addAction(m_previousAction);
    view->addAction(m_todayAction);
    view->addAction(m_nextAction);
    view->addSeparator();
    QAction *fullScreen = view->addAction(tr("&Full Screen"));
    fullScreen->setCheckable(true);
    fullScreen->setShortcut(QKeySequence(QStringLiteral("F11")));
    connect(fullScreen, &QAction::toggled, this, [this](bool on) {
        if (on) {
            showFullScreen();
        } else {
            showNormal();
        }
    });
    QAction *masjid = view->addAction(tr("&Masjid Screen"));
    masjid->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+M")));
    connect(masjid, &QAction::triggered, this, &MainWindow::showMasjid);
    view->addSeparator();
    m_qiblahAction = view->addAction(tr("Show &Qiblah"));
    m_qiblahAction->setCheckable(true);
    connect(m_qiblahAction, &QAction::toggled, this, [this](bool on) {
        namaz_core_set_flag(m_core.handle(), "show_qiblah", on ? 1 : 0);
        refreshFromCore();
    });
    m_hijriAction = view->addAction(tr("Show &Hijri Date"));
    m_hijriAction->setCheckable(true);
    connect(m_hijriAction, &QAction::toggled, this, [this](bool on) {
        namaz_core_set_flag(m_core.handle(), "hijri_calendar", on ? 1 : 0);
        refreshFromCore();
    });

    QMenu *playback = menuBar()->addMenu(tr("&Playback"));
    if (namaz_core_sound_exists("ADHAN")) {
        addSound(playback, tr("&Athan"), "ADHAN");
    }
    if (namaz_core_sound_exists("ALIHLAS")) {
        QMenu *quran = playback->addMenu(tr("&Quran"));
        addSound(quran, tr("Al-Ikhlas"), "ALIHLAS");
    }
    const bool hasDua = namaz_core_sound_exists("DUA")
        || namaz_core_sound_exists("DUA1")
        || namaz_core_sound_exists("DUA2");
    if (hasDua) {
        QMenu *duas = playback->addMenu(tr("&Duas"));
        if (namaz_core_sound_exists("DUA")) {
            addSound(duas, tr("Dua after Athan"), "DUA");
        }
        if (namaz_core_sound_exists("DUA1")) {
            addSound(duas, tr("Dua 1"), "DUA1");
        }
        if (namaz_core_sound_exists("DUA2")) {
            addSound(duas, tr("Dua 2"), "DUA2");
        }
    }
    if (namaz_core_sound_exists("BISMILLAH")) {
        addSound(playback, tr("&Bismi Allah"), "BISMILLAH");
    }
    QAction *prayNow = playback->addAction(
        Ui::themeIcon(QStringLiteral("media-playback-start")), tr("Pray &Now"));
    connect(prayNow, &QAction::triggered, this, &MainWindow::prayNow);

    playback->addSeparator();
    m_menuMuteAction = playback->addAction(tr("&Mute All"));
    m_menuMuteAction->setCheckable(true);
    connect(m_menuMuteAction, &QAction::toggled, this, [this](bool muted) {
        namaz_core_set_flag(m_core.handle(), "athan_enabled", muted ? 0 : 1);
        if (muted) {
            namaz_core_stop_audio();
        }
    });
    QAction *stop = playback->addAction(tr("&Stop Audio"));
    stop->setShortcut(QKeySequence(QStringLiteral("Ctrl+.")));
    stop->setIcon(Ui::themeIcon(QStringLiteral("media-playback-stop")));
    connect(stop, &QAction::triggered, [] { namaz_core_stop_audio(); });

    QMenu *settings = menuBar()->addMenu(tr("&Settings"));
    QAction *prayerSettings = settings->addAction(tr("Prayer &Settings..."));
    prayerSettings->setIcon(Ui::themeIcon(QStringLiteral("preferences-system-time")));
    QAction *preferences = settings->addAction(tr("&Preferences..."));
    preferences->setIcon(Ui::themeIcon(QStringLiteral("preferences-system")));
    preferences->setShortcut(QKeySequence::Preferences);
    preferences->setMenuRole(QAction::PreferencesRole);
    connect(prayerSettings, &QAction::triggered, this,
            [this] { openSettingsDialog(); });
    connect(preferences, &QAction::triggered, this,
            [this] { openPreferencesDialog(); });

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    QAction *updates = help->addAction(tr("Check for &Updates..."));
    connect(updates, &QAction::triggered, this, [this] { checkForUpdates(true); });
    help->addSeparator();
    QAction *about = help->addAction(tr("&About Namaz"));
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(
            this, tr("About Namaz"),
            tr("<b>Namaz</b><br>"
               "Muslim prayer times, Qibla and Hijri calendar.<br><br>"
               "Rust prayer engine + Qt 6 Widgets.<br>"
               "Author: Ikhlasulov<br>"
               "Inspired by Salaat Time 3.0.<br>"
               "Version %1")
                .arg(QCoreApplication::applicationVersion()));
    });
    QAction *aboutQt = help->addAction(tr("About &Qt"));
    aboutQt->setMenuRole(QAction::AboutQtRole);
    connect(aboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::buildShortcuts() {
    auto add = [this](const QString &sequence, auto callback) {
        auto *shortcut = new QShortcut(QKeySequence(sequence), this);
        connect(shortcut, &QShortcut::activated, this, callback);
    };
    add(QStringLiteral("PgUp"), [this] { shiftMonth(-1); });
    add(QStringLiteral("PgDown"), [this] { shiftMonth(1); });
    add(QStringLiteral("Home"), [this] { shiftYear(-1); });
    add(QStringLiteral("End"), [this] { shiftYear(1); });
    add(QStringLiteral("Esc"), [this] { goToday(); });

    // Day view: Left/Right step by one day.  They are disabled on Month and
    // Year so the tables keep their normal cursor movement.
    m_dayLeftShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    m_dayLeftShortcut->setContext(Qt::WindowShortcut);
    connect(m_dayLeftShortcut, &QShortcut::activated, this, [this] { shiftDay(-1); });
    m_dayRightShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    m_dayRightShortcut->setContext(Qt::WindowShortcut);
    connect(m_dayRightShortcut, &QShortcut::activated, this, [this] { shiftDay(1); });
    const bool dayView = m_views && m_views->currentIndex() == 0;
    m_dayLeftShortcut->setEnabled(dayView);
    m_dayRightShortcut->setEnabled(dayView);
}

void MainWindow::buildTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    QIcon icon = Ui::appIcon();
    m_tray = new QSystemTrayIcon(icon, this);
    m_tray->setIcon(icon);
    m_tray->setToolTip(QStringLiteral("Namaz"));

    auto *menu = new QMenu(this);
    QAction *open = menu->addAction(Ui::themeIcon(QStringLiteral("window-new")),
                                    tr("Open Namaz"));
    connect(open, &QAction::triggered, this, &MainWindow::showFromTray);
    m_trayNextAction = menu->addAction(tr("Next: —"));
    m_trayNextAction->setEnabled(false);
    QAction *pray = menu->addAction(Ui::themeIcon(QStringLiteral("media-playback-start")),
                                    tr("Pray Now"));
    connect(pray, &QAction::triggered, this, &MainWindow::prayNow);
    menu->addSeparator();

    QAction *settings = menu->addAction(
        Ui::themeIcon(QStringLiteral("preferences-system-time")),
        tr("Prayer Settings..."));
    connect(settings, &QAction::triggered, this, [this] { openSettingsDialog(); });
    QAction *preferences = menu->addAction(
        Ui::themeIcon(QStringLiteral("preferences-system")),
        tr("Preferences..."));
    connect(preferences, &QAction::triggered, this, [this] { openPreferencesDialog(); });
    menu->addSeparator();

    QMenu *playback = menu->addMenu(tr("Playback"));
    if (namaz_core_sound_exists("ADHAN")) {
        QAction *athan = playback->addAction(tr("Athan"));
        connect(athan, &QAction::triggered, this, [this] { playSound(QStringLiteral("ADHAN")); });
    }
    if (namaz_core_sound_exists("ALIHLAS")) {
        QAction *sura = playback->addAction(tr("Al-Ikhlas"));
        connect(sura, &QAction::triggered, this, [this] { playSound(QStringLiteral("ALIHLAS")); });
    }
    if (namaz_core_sound_exists("BISMILLAH")) {
        QAction *bismillah = playback->addAction(tr("Bismi Allah"));
        connect(bismillah, &QAction::triggered, this,
                [this] { playSound(QStringLiteral("BISMILLAH")); });
    }
    QAction *stop = playback->addAction(Ui::themeIcon(QStringLiteral("media-playback-stop")),
                                        tr("Stop Audio"));
    connect(stop, &QAction::triggered, [] { namaz_core_stop_audio(); });

    const QJsonObject cfg = config();
    m_muteAction = menu->addAction(tr("Mute All"));
    m_muteAction->setCheckable(true);
    m_muteAction->setChecked(!cfg.value(QStringLiteral("athan_enabled")).toBool(true));
    connect(m_muteAction, &QAction::toggled, this, [this](bool muted) {
        namaz_core_set_flag(m_core.handle(), "athan_enabled", muted ? 0 : 1);
        if (muted) {
            namaz_core_stop_audio();
        }
    });
    menu->addSeparator();
    QAction *quit = menu->addAction(Ui::themeIcon(QStringLiteral("application-exit")),
                                    tr("Quit"));
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick) {
                    showFromTray();
                }
            });
    m_tray->show();
}

bool MainWindow::isTrayAvailable() const {
    return m_tray != nullptr;
}

bool MainWindow::startMinimized() const {
    const QJsonObject cfg = config();
    return cfg.value(QStringLiteral("launch_minimized")).toBool(false);
}

bool MainWindow::shouldShowSplash() const {
    const QJsonObject cfg = config();
    return cfg.value(QStringLiteral("show_splash")).toBool(false);
}

void MainWindow::showFromTray() {
    // showNormal() would also clear the maximized/fullscreen state, so only
    // leave the minimized state that a tray restore actually needs.
    if (isMinimized()) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    show();
    raise();
    activateWindow();
}

void MainWindow::setTab(int index) {
    if (!m_views) {
        return;
    }
    const int bounded = qBound(0, index, m_views->count() - 1);
    m_views->setCurrentIndex(bounded);
    if (bounded >= 0 && bounded < 3 && m_viewActions[bounded]) {
        // The action group unchecks its previous member through the action's
        // own signals; do not block them.
        m_viewActions[bounded]->setChecked(true);
    }
}

void MainWindow::tick() {
    const QString eventsText = takeCoreString(
        namaz_core_tick_events(m_core.handle()));
    const QJsonArray events = QJsonDocument::fromJson(eventsText.toUtf8()).array();
    if (!events.isEmpty()) {
        const QJsonObject cfg = config();
        for (const QJsonValue &value : events) {
            const QString type = value.toObject().value(QStringLiteral("type")).toString();
            if (type == QStringLiteral("athan")) {
                if (cfg.value(QStringLiteral("athan_visual_main")).toBool(false)) {
                    showFromTray();
                }
                if (cfg.value(QStringLiteral("athan_visual_flash")).toBool(false)) {
                    flashTray();
                }
            } else if (type == QStringLiteral("reminder")) {
                if (cfg.value(QStringLiteral("reminder_visual_main")).toBool(false)) {
                    showFromTray();
                }
                if (cfg.value(QStringLiteral("reminder_visual_flash")).toBool(false)) {
                    flashTray();
                }
            }
        }
    }
    refreshFromCore();
}

void MainWindow::playStartupSound() {
    namaz_core_startup(m_core.handle());
}

QJsonObject MainWindow::config() const {
    const QString text = takeCoreString(namaz_core_config_json(m_core.handle()));
    return QJsonDocument::fromJson(text.toUtf8()).object();
}

void MainWindow::refreshFromCore() {
    const QString text = takeCoreString(namaz_core_day_json(m_core.handle()));
    if (text.isEmpty()) {
        return;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }
    const QJsonObject root = document.object();
    const QJsonObject date = root.value(QStringLiteral("date")).toObject();
    m_selectedDate = QDate::fromString(date.value(QStringLiteral("iso")).toString(),
                                      Qt::ISODate);

    if (m_dayView) {
        m_dayView->setState(root);
    }
    if (m_cityLabel) {
        m_cityLabel->setText(root.value(QStringLiteral("city")).toString());
    }

    const QJsonObject cfg = config();
    if (m_methodLabel) {
        const int method = cfg.value(QStringLiteral("method")).toInt(1);
        m_methodLabel->setText(
            tr("Method: %1").arg(Ui::methodShortName(method)));
        m_methodLabel->setToolTip(Ui::methodName(method));
    }

    const int currentIndex = root.value(QStringLiteral("current"))
                                 .toObject()
                                 .value(QStringLiteral("index"))
                                 .toInt(-1);
    if (currentIndex >= 0) {
        setWindowTitle(tr("Namaz - Current Prayer is %1")
                           .arg(Ui::prayerName(currentIndex)));
    } else {
        setWindowTitle(tr("Namaz"));
    }

    for (MasjidWindow *window : m_masjidWindows) {
        if (window) {
            window->setState(root);
        }
    }
    if (m_dbus
        && cfg.value(QStringLiteral("dbus_enabled")).toBool(false)) {
        m_dbus->update(root,
                       cfg.value(QStringLiteral("dbus_interval")).toInt(60));
    }
    updateTray();
}

void MainWindow::refreshCalendar() {
    if (!m_views) {
        return;
    }
    const int view = m_views->currentIndex();
    namaz_core_set_view(m_core.handle(), view);
    if (view == 1 && m_monthView) {
        const QString dayText = takeCoreString(namaz_core_day_json(m_core.handle()));
        const QJsonObject day = QJsonDocument::fromJson(dayText.toUtf8()).object();
        const QString monthText = takeCoreString(
            namaz_core_month_json(m_core.handle()));
        const QJsonObject month = QJsonDocument::fromJson(monthText.toUtf8()).object();
        m_monthView->setData(month, day);
    } else if (view == 2 && m_yearView) {
        const QString yearText = takeCoreString(
            namaz_core_year_json(m_core.handle()));
        const QJsonObject year = QJsonDocument::fromJson(yearText.toUtf8()).object();
        m_yearView->setData(year);
    }
}

void MainWindow::refreshAll() {
    refreshFromCore();
    refreshCalendar();
}

void MainWindow::shiftDay(int days) {
    namaz_core_shift_day(m_core.handle(), days);
    refreshAll();
}

void MainWindow::shiftMonth(int months) {
    namaz_core_shift_month(m_core.handle(), months);
    refreshAll();
}

void MainWindow::shiftYear(int years) {
    namaz_core_shift_year(m_core.handle(), years);
    refreshAll();
}

void MainWindow::goToday() {
    namaz_core_go_today(m_core.handle());
    refreshAll();
}

void MainWindow::setSelectedDate(const QDate &date) {
    if (!date.isValid()) {
        return;
    }
    const QByteArray iso = date.toString(Qt::ISODate).toUtf8();
    namaz_core_set_date(m_core.handle(), iso.constData());
    refreshAll();
}

void MainWindow::updateTray() {
    if (!m_tray) {
        return;
    }
    const QString text = takeCoreString(namaz_core_day_json(m_core.handle()));
    const QJsonObject root = QJsonDocument::fromJson(text.toUtf8()).object();
    const QJsonObject next = root.value(QStringLiteral("next")).toObject();
    const QString name = Ui::prayerName(
        next.value(QStringLiteral("index")).toInt(-1));
    const int seconds = next.value(QStringLiteral("seconds")).toInt(-1);
    const QString nextText = seconds >= 0
                                 ? tr("%1 %2").arg(name, Ui::formatTime(seconds))
                                 : tr("—");
    m_tray->setToolTip(tr("Namaz — Next: %1").arg(nextText));
    if (m_trayNextAction) {
        m_trayNextAction->setText(tr("Next: %1").arg(nextText));
    }
    if (m_muteAction || m_menuMuteAction) {
        const QJsonObject cfg = config();
        const bool muted = !cfg.value(QStringLiteral("athan_enabled")).toBool(true);
        if (m_muteAction && m_muteAction->isChecked() != muted) {
            const QSignalBlocker blocker(m_muteAction);
            m_muteAction->setChecked(muted);
        }
        if (m_menuMuteAction && m_menuMuteAction->isChecked() != muted) {
            const QSignalBlocker blocker(m_menuMuteAction);
            m_menuMuteAction->setChecked(muted);
        }
    }
}

void MainWindow::flashTray() {
    if (!m_tray) {
        return;
    }
    if (m_flashTimer) {
        m_flashTimer->stop();
        m_flashTimer->deleteLater();
    }
    auto *timer = new QTimer(this);
    m_flashTimer = timer;
    timer->setInterval(500);
    m_flashOn = false;
    const QIcon normal = windowIcon();
    const QIcon alert = Ui::themeIcon(QStringLiteral("dialog-information"));
    connect(timer, &QTimer::timeout, this, [this, normal, alert] {
        m_flashOn = !m_flashOn;
        if (m_tray) {
            m_tray->setIcon(m_flashOn ? alert : normal);
        }
    });
    timer->start();
    QTimer::singleShot(5 * 60 * 1000, this, [this, timer, normal] {
        // An older flash cycle must not cancel a newer one.
        if (m_flashTimer != timer) {
            return;
        }
        timer->stop();
        timer->deleteLater();
        m_flashTimer = nullptr;
        if (m_tray) {
            m_tray->setIcon(normal);
        }
    });
}

void MainWindow::playSound(const QString &name) {
    if (!namaz_core_play_sound(name.toUtf8().constData())) {
        showAudioError(name);
    }
}

void MainWindow::showAudioError(const QString &name) {
    statusBar()->showMessage(tr("Audio file not found: %1").arg(name), 5000);
}

void MainWindow::printMonth(bool toPdf, int calendar) {
    const QDate date = m_selectedDate.isValid() ? m_selectedDate : QDate::currentDate();
    QString csv;
    QString monthTitle;
    int fileYear = date.year();
    int fileMonth = date.month();
    if (calendar == 1) {
        const QJsonObject month = QJsonDocument::fromJson(
            takeCoreString(namaz_core_month_json(m_core.handle())).toUtf8()).object();
        const int hijriYear = month.value(QStringLiteral("hijriYear")).toInt();
        const int hijriMonth = month.value(QStringLiteral("hijriMonth")).toInt();
        csv = takeCoreString(
            namaz_core_csv_hijri_month(m_core.handle(), hijriYear, hijriMonth));
        monthTitle = Ui::hijriMonthName(hijriMonth);
        fileYear = hijriYear;
        fileMonth = hijriMonth;
    } else {
        csv = takeCoreString(namaz_core_csv_month_calendar(
            m_core.handle(), date.year(), date.month(), calendar));
        monthTitle = QLocale().standaloneMonthName(date.month(), QLocale::LongFormat);
    }
    if (csv.isEmpty()) {
        QMessageBox::warning(this, tr("Print"),
                             tr("Could not calculate prayer times."));
        return;
    }

    const QString calendarName = calendar == 1 ? tr("Hijri") : tr("Gregorian");
    QPrinter printer;
    printer.setDocName(tr("Namaz — %1 %2 (%3)")
                           .arg(monthTitle, QString::number(fileYear), calendarName));
    if (toPdf) {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Print to PDF"),
            QDir::homePath() + QStringLiteral("/Namaz-%1-%2-%3.pdf")
                                   .arg(fileYear)
                                   .arg(fileMonth, 2, 10, QLatin1Char('0'))
                                   .arg(calendar == 1 ? QStringLiteral("hijri")
                                                      : QStringLiteral("gregorian")),
            tr("PDF files (*.pdf)"));
        if (path.isEmpty()) {
            return;
        }
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(path);
    } else {
        QPrintDialog dialog(&printer, this);
        dialog.setWindowTitle(tr("Print Prayer Times"));
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
    }

    const QJsonObject cfg = config();
    const QString city = cfg.value(QStringLiteral("city")).toString();
    const QString zone = cfg.value(QStringLiteral("timezone_name")).toString();
    const QString subtitle = zone.isEmpty() ? city : tr("%1 — %2").arg(city, zone);
    QTextDocument document;
    document.setHtml(htmlForCsv(csv, printer.docName(), subtitle));
    document.print(&printer);
}

bool MainWindow::printMonthToPath(const QString &path, int calendar) {
    const QDate date = m_selectedDate.isValid() ? m_selectedDate : QDate::currentDate();
    QString csv;
    QString monthTitle;
    int fileYear = date.year();
    if (calendar == 1) {
        const QJsonObject month = QJsonDocument::fromJson(
            takeCoreString(namaz_core_month_json(m_core.handle())).toUtf8()).object();
        const int hijriYear = month.value(QStringLiteral("hijriYear")).toInt();
        const int hijriMonth = month.value(QStringLiteral("hijriMonth")).toInt();
        csv = takeCoreString(
            namaz_core_csv_hijri_month(m_core.handle(), hijriYear, hijriMonth));
        monthTitle = Ui::hijriMonthName(hijriMonth);
        fileYear = hijriYear;
    } else {
        csv = takeCoreString(namaz_core_csv_month_calendar(
            m_core.handle(), date.year(), date.month(), calendar));
        monthTitle = QLocale().standaloneMonthName(date.month(), QLocale::LongFormat);
    }
    if (csv.isEmpty() || path.isEmpty()) {
        return false;
    }
    const QJsonObject cfg = config();
    const QString city = cfg.value(QStringLiteral("city")).toString();
    const QString zone = cfg.value(QStringLiteral("timezone_name")).toString();
    const QString subtitle = zone.isEmpty() ? city : tr("%1 — %2").arg(city, zone);
    QFile::remove(path);
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setDocName(tr("Namaz — %1 %2")
                           .arg(monthTitle, QString::number(fileYear)));
    QTextDocument document;
    document.setHtml(htmlForCsv(csv, printer.docName(), subtitle));
    document.print(&printer);
    const QFileInfo written(path);
    return written.exists() && written.size() > 0;
}

void MainWindow::exportCsvMonth(int month, int calendar) {
    const QDate date = m_selectedDate.isValid() ? m_selectedDate : QDate::currentDate();
    QString text;
    int fileYear = date.year();
    if (calendar == 1) {
        const QJsonObject day = QJsonDocument::fromJson(
            takeCoreString(namaz_core_day_json(m_core.handle())).toUtf8()).object();
        const int hijriYear = day.value(QStringLiteral("date"))
                                  .toObject()
                                  .value(QStringLiteral("hijriYear"))
                                  .toInt();
        text = takeCoreString(
            namaz_core_csv_hijri_month(m_core.handle(), hijriYear, month));
        fileYear = hijriYear;
    } else {
        text = takeCoreString(namaz_core_csv_month_calendar(
            m_core.handle(), date.year(), month, calendar));
    }
    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Could not calculate prayer times."));
        return;
    }
    const QString suffix = calendar == 1 ? QStringLiteral("hijri")
                           : calendar == 2 ? QStringLiteral("outlook")
                                           : QStringLiteral("gregorian");
    const QString suggested = QDir::homePath()
        + QStringLiteral("/Namaz-%1-%2-%3.csv")
              .arg(fileYear)
              .arg(month, 2, 10, QLatin1Char('0'))
              .arg(suffix);
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Prayer Times"), suggested,
        tr("CSV files (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Could not write %1").arg(path));
        return;
    }
    const QByteArray payload = text.toUtf8();
    if (file.write(payload) != payload.size() || !file.flush()) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Could not write %1").arg(path));
        return;
    }
    file.close();
    QMessageBox::information(this, tr("Export"),
                             tr("Prayer times exported to\n%1").arg(path));
}

void MainWindow::exportCsvYear(int calendar) {
    const QDate date = m_selectedDate.isValid() ? m_selectedDate : QDate::currentDate();
    QString text;
    int fileYear = date.year();
    if (calendar == 1) {
        const QJsonObject day = QJsonDocument::fromJson(
            takeCoreString(namaz_core_day_json(m_core.handle())).toUtf8()).object();
        const int hijriYear = day.value(QStringLiteral("date"))
                                  .toObject()
                                  .value(QStringLiteral("hijriYear"))
                                  .toInt();
        text = takeCoreString(
            namaz_core_csv_hijri_year(m_core.handle(), hijriYear));
        fileYear = hijriYear;
    } else {
        text = takeCoreString(
            namaz_core_csv_year_calendar(m_core.handle(), date.year(), calendar));
    }
    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Could not calculate prayer times."));
        return;
    }
    const QString suffix = calendar == 1 ? QStringLiteral("hijri")
                           : calendar == 2 ? QStringLiteral("outlook")
                                           : QStringLiteral("gregorian");
    const QString suggested = QDir::homePath()
        + QStringLiteral("/Namaz-%1-%2.csv")
              .arg(fileYear)
              .arg(suffix);
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Prayer Times"), suggested,
        tr("CSV files (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Could not write %1").arg(path));
        return;
    }
    const QByteArray payload = text.toUtf8();
    if (file.write(payload) != payload.size() || !file.flush()) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Could not write %1").arg(path));
        return;
    }
    file.close();
    QMessageBox::information(this, tr("Export"),
                             tr("Prayer times exported to\n%1").arg(path));
}

void MainWindow::showMasjid() {
    qDeleteAll(m_masjidWindows);
    m_masjidWindows.clear();

    const QJsonObject cfg = config();
    const int requested = cfg.value(QStringLiteral("masjid_screen")).toInt(0);
    const auto screens = QGuiApplication::screens();
    QList<QScreen *> targets;
    if (requested == -1) {
        targets = screens;
    } else if (requested > 0 && requested <= screens.size()) {
        targets.append(screens.at(requested - 1));
    } else {
        targets.append(QGuiApplication::primaryScreen());
    }

    for (QScreen *screen : targets) {
        auto *window = new MasjidWindow(nullptr);
        window->setScreen(screen);
        window->showFullScreen();
        m_masjidWindows.append(window);
    }
    refreshFromCore();
}

void MainWindow::prayNow() {
    const QJsonObject cfg = config();
    if (cfg.value(QStringLiteral("lock_on_pray_now")).toBool(false)) {
        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.ScreenSaver"),
            QStringLiteral("/org/freedesktop/ScreenSaver"),
            QStringLiteral("org.freedesktop.ScreenSaver"),
            QStringLiteral("Lock"));
        QDBusConnection::sessionBus().send(message);
    }
    if (cfg.value(QStringLiteral("screensaver_on_pray_now")).toBool(false)) {
        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.ScreenSaver"),
            QStringLiteral("/org/freedesktop/ScreenSaver"),
            QStringLiteral("org.freedesktop.ScreenSaver"),
            QStringLiteral("SetActive"));
        message << true;
        QDBusConnection::sessionBus().send(message);
    }
    showMasjid();
}

void MainWindow::openSettingsDialog(int tab) {
    if (!m_settingsDialog) {
        m_settingsDialog = new PrayerSettingsDialog(m_core.handle(), this);
        connect(m_settingsDialog, &PrayerSettingsDialog::applied, this, [this] {
            refreshAll();
            applyPreferencesToWindow();
        });
    }
    // Cancel must not leave stale edits in the reused dialog.
    if (!m_settingsDialog->isVisible()) {
        m_settingsDialog->reload();
    }
    m_settingsDialog->setTabIndex(tab);
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void MainWindow::openPreferencesDialog(int tab) {
    if (!m_preferencesDialog) {
        m_preferencesDialog = new PreferencesDialog(m_core.handle(), this);
        connect(m_preferencesDialog, &PreferencesDialog::applied, this, [this] {
            refreshAll();
            applyPreferencesToWindow();
        });
    }
    if (!m_preferencesDialog->isVisible()) {
        m_preferencesDialog->reload();
    }
    m_preferencesDialog->setTabIndex(tab);
    m_preferencesDialog->show();
    m_preferencesDialog->raise();
    m_preferencesDialog->activateWindow();
}

void MainWindow::applyPreferencesToWindow() {
    const QJsonObject cfg = config();
    QString locale = cfg.value(QStringLiteral("locale")).toString();
    if (locale.isEmpty()) {
        locale = QStringLiteral("system");
    }
    if (!m_locale.isEmpty() && locale != m_locale) {
        m_locale = locale;
        restartForLanguage();
        return;
    }
    const bool alwaysOnTop = cfg.value(QStringLiteral("always_on_top")).toBool(false);
    if (isVisible()) {
        const bool currentlyOnTop = windowFlags().testFlag(Qt::WindowStaysOnTopHint);
        if (currentlyOnTop != alwaysOnTop) {
            setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
            show();
        }
    } else {
        setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
    }

    if (m_dbus) {
        if (cfg.value(QStringLiteral("dbus_enabled")).toBool(false)) {
            m_dbus->enable();
        } else {
            m_dbus->disable();
        }
    }
    if (m_qiblahAction) {
        const QSignalBlocker blocker(m_qiblahAction);
        m_qiblahAction->setChecked(
            cfg.value(QStringLiteral("show_qiblah")).toBool(true));
    }
    if (m_hijriAction) {
        const QSignalBlocker blocker(m_hijriAction);
        m_hijriAction->setChecked(
            cfg.value(QStringLiteral("hijri_calendar")).toBool(true));
    }
    if (m_menuMuteAction) {
        const QSignalBlocker blocker(m_menuMuteAction);
        m_menuMuteAction->setChecked(
            !cfg.value(QStringLiteral("athan_enabled")).toBool(true));
    }
    namaz_core_set_volume(cfg.value(QStringLiteral("volume")).toInt(90));
    syncAutostart();
}

void MainWindow::syncAutostart() {
    const QJsonObject cfg = config();
    const bool enabled = cfg.value(QStringLiteral("launch_on_startup")).toBool(false);
    // Inside Flatpak $HOME/.config is a per-run tmpfs; autostart must be
    // written to the host directory (granted by xdg-config/autostart) and
    // must launch the app through flatpak, not the sandbox path /app/bin.
    const bool flatpak = qEnvironmentVariableIsSet("FLATPAK_ID");
    const QString autostartDir =
        flatpak
            ? QDir::homePath() + QStringLiteral("/.config/autostart")
            : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                  + QStringLiteral("/autostart");
    const QString path = autostartDir
        + QStringLiteral("/io.github.ikhlasulov_gb.Namaz.desktop");
    // Remove the entry written by the old working title so a user does not
    // start two instances after upgrading.
    QFile::remove(autostartDir
                  + QStringLiteral("/io.github.ikhlasulov.SalaatTime.desktop"));
    if (!enabled) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(autostartDir);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning("autostart: cannot write %s", qUtf8Printable(path));
        return;
    }
    QString execLine;
    if (flatpak) {
        const QString id =
            qEnvironmentVariable("FLATPAK_ID",
                                 QStringLiteral("io.github.ikhlasulov_gb.Namaz"));
        QString flatpakProgram = QStringLiteral("/usr/bin/flatpak");
        if (!QFileInfo::exists(flatpakProgram)) {
            flatpakProgram = QStringLiteral("flatpak");
        }
        execLine = QStringLiteral("%1 run %2").arg(flatpakProgram, id);
    } else {
        const QString executable = QCoreApplication::applicationFilePath();
        const QString applicationDir = QCoreApplication::applicationDirPath();
        const QString libraryPath = applicationDir + QLatin1Char(':')
            + QDir(applicationDir).filePath(QStringLiteral("../lib"));
        execLine = QStringLiteral("env LD_LIBRARY_PATH=\"%1\" \"%2\"")
                       .arg(libraryPath, executable);
    }
    const QString text =
        QStringLiteral("[Desktop Entry]\n"
                       "Type=Application\n"
                       "Name=Namaz\n"
                       "Comment=Muslim prayer times, Qibla and Hijri calendar\n"
                       "Exec=%1\n"
                       "Icon=io.github.ikhlasulov_gb.Namaz\n"
                       "Terminal=false\n"
                       "StartupNotify=false\n"
                       "X-GNOME-Autostart-enabled=true\n"
                       "X-KDE-autostart-after=panel\n")
            .arg(execLine);
    file.write(text.toUtf8());
}

void MainWindow::restartForLanguage() {
    const QString program = QCoreApplication::applicationFilePath();
    QStringList arguments = QCoreApplication::arguments().mid(1);
    arguments.removeAll(QStringLiteral("--settings"));
    arguments.removeAll(QStringLiteral("--preferences"));
    arguments.removeAll(QStringLiteral("--masjid"));

    auto quote = [](const QString &value) {
        QString escaped = value;
        escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
        return QLatin1Char('\'') + escaped + QLatin1Char('\'');
    };
    QString command = QStringLiteral("sleep 1; exec %1").arg(quote(program));
    for (const QString &argument : arguments) {
        command += QLatin1Char(' ') + quote(argument);
    }
    QProcess::startDetached(QStringLiteral("/bin/sh"),
                            {QStringLiteral("-c"), command});
    QCoreApplication::quit();
}

void MainWindow::checkForUpdates(bool manual) {
    if (!m_updater) {
        return;
    }
    if (UpdateChecker::isFlatpak()) {
        // The Flatpak sandbox has no network permission; updates are handled
        // by the signed OSTree remote and the desktop software centre.
        if (manual) {
            QMessageBox::information(
                this, tr("Check for Updates"),
                tr("Namaz is installed as a Flatpak. Use your software centre "
                   "or run:\nflatpak update --user io.github.ikhlasulov_gb.Namaz"));
        }
        return;
    }
    if (manual) {
        statusBar()->showMessage(tr("Checking for updates..."), 5000);
    }
    m_updater->check(manual);
}

void MainWindow::showUpdateDialog(const UpdateChecker::Release &release) {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Update Available"));
    auto *layout = new QVBoxLayout(&dialog);

    auto *title = new QLabel(
        tr("<b>Namaz %1 is available.</b><br>You are using %2.")
            .arg(release.version, QCoreApplication::applicationVersion()),
        &dialog);
    title->setTextFormat(Qt::RichText);
    layout->addWidget(title);

    auto *notes = new QTextBrowser(&dialog);
    notes->setOpenExternalLinks(true);
    notes->setPlainText(release.body.isEmpty()
                            ? tr("No release notes were provided.")
                            : release.body);
    notes->setMinimumHeight(180);
    layout->addWidget(notes);

    if (UpdateChecker::isFlatpak()) {
        auto *hint = new QLabel(
            tr("Namaz is installed as a Flatpak. Use your software centre or run:\n"
               "flatpak update --user io.github.ikhlasulov_gb.Namaz"),
            &dialog);
        hint->setWordWrap(true);
        layout->addWidget(hint);
    }

    auto *buttons = new QDialogButtonBox(&dialog);
    QPushButton *open = buttons->addButton(tr("Open release page"),
                                           QDialogButtonBox::ActionRole);
    QPushButton *install = nullptr;
    if (UpdateChecker::isAppImage()
        && !UpdateChecker::releaseAssetUrl(release, QStringLiteral(".AppImage"))
                .isEmpty()) {
        install = buttons->addButton(tr("Download and install"),
                                     QDialogButtonBox::AcceptRole);
    }
    QPushButton *copy = nullptr;
    if (UpdateChecker::isFlatpak()) {
        copy = buttons->addButton(tr("Copy update command"),
                                  QDialogButtonBox::ActionRole);
    }
    QPushButton *skip = buttons->addButton(tr("Skip this version"),
                                           QDialogButtonBox::DestructiveRole);
    QPushButton *later = buttons->addButton(tr("Later"),
                                            QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    bool installRequested = false;
    connect(open, &QPushButton::clicked, &dialog, [&] {
        if (!release.htmlUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(release.htmlUrl));
        }
        dialog.accept();
    });
    if (copy) {
        connect(copy, &QPushButton::clicked, &dialog, [&] {
            QGuiApplication::clipboard()->setText(
                QStringLiteral("flatpak update --user io.github.ikhlasulov_gb.Namaz"));
            QMessageBox::information(&dialog, tr("Update command copied"),
                                     tr("The command was copied to the clipboard."));
        });
    }
    if (install) {
        connect(install, &QPushButton::clicked, &dialog, [&] {
            installRequested = true;
            dialog.accept();
        });
    }
    connect(skip, &QPushButton::clicked, &dialog, [&] {
        QSettings().setValue(QStringLiteral("updates/skippedVersion"),
                             release.version);
        dialog.reject();
    });
    connect(later, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
    if (installRequested) {
        installAppImageUpdate(release);
    }
}

void MainWindow::installAppImageUpdate(const UpdateChecker::Release &release) {
    const QString assetUrlText =
        UpdateChecker::releaseAssetUrl(release, QStringLiteral(".AppImage"));
    if (assetUrlText.isEmpty()) {
        if (!release.htmlUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(release.htmlUrl));
        }
        return;
    }

    const QUrl assetUrl(assetUrlText);
    const QString assetName = QFileInfo(assetUrl.path()).fileName();
    QNetworkAccessManager manager;
    auto fetch = [&](const QUrl &url, QByteArray *data) -> bool {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = manager.get(request);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        const bool ok = reply->error() == QNetworkReply::NoError;
        if (ok) {
            *data = reply->readAll();
        }
        reply->deleteLater();
        return ok;
    };

    QProgressDialog progress(tr("Downloading Namaz %1...").arg(release.version),
                             tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();
    QByteArray payload;
    const bool downloaded = fetch(assetUrl, &payload);
    progress.close();
    if (!downloaded) {
        QMessageBox::warning(this, tr("Update failed"),
                             tr("The update could not be downloaded."));
        return;
    }

    const QString sumsUrl =
        UpdateChecker::releaseAssetUrl(release, QStringLiteral("SHA256SUMS"));
    QString expected;
    if (!sumsUrl.isEmpty()) {
        QByteArray sums;
        if (fetch(QUrl(sumsUrl), &sums)) {
            const QStringList lines =
                QString::fromUtf8(sums).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                                     Qt::SkipEmptyParts);
                if (parts.size() >= 2
                    && QFileInfo(parts.at(1)).fileName() == assetName) {
                    expected = parts.at(0).trimmed().toLower();
                    break;
                }
            }
        }
    }
    if (!expected.isEmpty()) {
        const QString actual = QString::fromLatin1(
            QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
        if (actual != expected) {
            QMessageBox::critical(
                this, tr("Update failed"),
                tr("The downloaded file does not match its SHA-256 checksum."));
            return;
        }
    }

    QTemporaryFile temp(QDir::tempPath()
                        + QStringLiteral("/Namaz-update-XXXXXX.AppImage"));
    temp.setAutoRemove(false);
    if (!temp.open() || temp.write(payload) != payload.size()) {
        QMessageBox::warning(this, tr("Update failed"),
                             tr("Could not write the downloaded file."));
        return;
    }
    temp.close();
    const QString tempPath = temp.fileName();
    QFile::setPermissions(tempPath, QFile::ReadOwner | QFile::WriteOwner
                                        | QFile::ExeOwner | QFile::ReadUser
                                        | QFile::ExeUser | QFile::ReadGroup
                                        | QFile::ExeGroup | QFile::ReadOther
                                        | QFile::ExeOther);

    QString target = qEnvironmentVariable("APPIMAGE");
    if (target.isEmpty()) {
        target = QCoreApplication::applicationFilePath();
    }
    if (!QFileInfo(target).isWritable()) {
        target = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
            + QLatin1Char('/') + assetName;
    }
    const QString backup = target + QStringLiteral(".old");
    QFile::remove(backup);
    const bool moved = QFile::rename(target, backup);
    const bool installed = moved && QFile::rename(tempPath, target);
    if (!installed) {
        if (moved) {
            QFile::rename(backup, target);
        }
        const QString fallback =
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
            + QLatin1Char('/') + assetName;
        QFile::copy(tempPath, fallback);
        QMessageBox::information(
            this, tr("Update downloaded"),
            tr("The new version was saved to:\n%1\n\nStart it manually to "
               "replace the current AppImage.")
                .arg(fallback));
        return;
    }
    QFile::remove(backup);

    if (QMessageBox::question(
            this, tr("Update installed"),
            tr("Namaz %1 has been installed. Restart now?").arg(release.version),
            QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
        QProcess::startDetached(target, {});
        QCoreApplication::quit();
    }
}

void MainWindow::saveWindowState() {
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.setValue(QStringLiteral("window/layoutVersion"), 3);
    if (m_views) {
        settings.setValue(QStringLiteral("window/tab"), m_views->currentIndex());
    }
}

void MainWindow::restoreWindowState() {
    QSettings settings;
    const int layoutVersion =
        settings.value(QStringLiteral("window/layoutVersion"), 0).toInt();
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (layoutVersion >= 3 && !geometry.isEmpty()) {
        restoreGeometry(geometry);
    } else {
        resize(initialWindowSize());
    }
    const QByteArray state = settings.value(QStringLiteral("window/state")).toByteArray();
    if (!state.isEmpty()) {
        restoreState(state);
    }
    const int tab = settings.value(QStringLiteral("window/tab"), 0).toInt();
    if (tab > 0) {
        setTab(tab);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveWindowState();
    const QJsonObject cfg = config();
    if (cfg.value(QStringLiteral("hide_on_close")).toBool(false)
        && QSystemTrayIcon::isSystemTrayAvailable()) {
        event->ignore();
        hide();
        return;
    }
    // Do not leave independent Masjid windows alive after the main window
    // has really been closed, or the application would never quit.
    for (MasjidWindow *window : m_masjidWindows) {
        if (window) {
            window->hide();
            window->close();
        }
    }
    m_masjidWindows.clear();
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!m_centralLayout || !centralWidget()) {
        return;
    }
    constexpr int kMaxContentWidth = 1120;
    const int available = centralWidget()->width();
    const int margin = qMax(0, (available - kMaxContentWidth) / 2);
    if (m_centralLayout->contentsMargins().left() != margin) {
        m_centralLayout->setContentsMargins(margin, 0, margin, 0);
    }
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() != QEvent::WindowStateChange || !isMinimized()) {
        return;
    }
    const QJsonObject cfg = config();
    if (cfg.value(QStringLiteral("hide_on_close")).toBool(false)
        && QSystemTrayIcon::isSystemTrayAvailable()) {
        QTimer::singleShot(0, this, [this] { hide(); });
    }
}
