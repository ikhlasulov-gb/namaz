#include "Widgets.h"

#include "UiUtils.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QSize>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QWidget>

#include <cstdio>

namespace {

QJsonObject rectJson(const QRect &rect) {
    return QJsonObject{
        {QStringLiteral("x"), rect.x()},
        {QStringLiteral("y"), rect.y()},
        {QStringLiteral("w"), rect.width()},
        {QStringLiteral("h"), rect.height()},
    };
}

QJsonObject dumpWidget(QWidget *widget, QWidget *origin) {
    const QPoint pos = widget->mapTo(origin, QPoint(0, 0));
    QJsonObject object{
        {QStringLiteral("name"), widget->objectName()},
        {QStringLiteral("class"), QString::fromLatin1(widget->metaObject()->className())},
        {QStringLiteral("x"), pos.x()},
        {QStringLiteral("y"), pos.y()},
        {QStringLiteral("w"), widget->width()},
        {QStringLiteral("h"), widget->height()},
    };

    if (auto *table = qobject_cast<QTableWidget *>(widget)) {
        object.insert(QStringLiteral("rows"), table->rowCount());
        object.insert(QStringLiteral("columns"), table->columnCount());
        object.insert(QStringLiteral("viewportHeight"), table->viewport()->height());
        QJsonArray widths;
        for (int column = 0; column < table->columnCount(); ++column) {
            widths.append(table->columnWidth(column));
        }
        object.insert(QStringLiteral("columnWidths"), widths);
    }

    if (auto *tabs = qobject_cast<QTabWidget *>(widget)) {
        object.insert(QStringLiteral("currentIndex"), tabs->currentIndex());
        object.insert(QStringLiteral("documentMode"), tabs->documentMode());
    }

    if (auto *tabBar = qobject_cast<QTabBar *>(widget)) {
        QJsonArray tabRects;
        for (int index = 0; index < tabBar->count(); ++index) {
            tabRects.append(rectJson(tabBar->tabRect(index)));
        }
        object.insert(QStringLiteral("tabs"), tabRects);
        object.insert(QStringLiteral("expanding"), tabBar->expanding());
    }

    QJsonArray children;
    const auto direct = widget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : direct) {
        children.append(dumpWidget(child, origin));
    }
    if (!children.isEmpty()) {
        object.insert(QStringLiteral("children"), children);
    }
    return object;
}

}  // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Namaz"));
    QApplication::setApplicationVersion(QStringLiteral(NAMAZ_VERSION));
    QApplication::setOrganizationName(QStringLiteral("io.github.ikhlasulov_gb"));
    QApplication::setOrganizationDomain(QStringLiteral("github.com/ikhlasulov-gb"));
    QApplication::setDesktopFileName(QStringLiteral("io.github.ikhlasulov_gb.Namaz"));

    // QSettings keys from the old working title (window geometry, selected
    // tab).  Copy them once so the first Namaz start looks familiar.
    {
        QSettings current;
        if (current.allKeys().isEmpty()) {
            QSettings legacy(QStringLiteral("io.github.ikhlasulov"),
                             QStringLiteral("SalaatTime"));
            const QStringList keys = legacy.allKeys();
            for (const QString &key : keys) {
                current.setValue(key, legacy.value(key));
            }
            current.sync();
        }
    }

    QString locale;
    {
        Core bootCore;
        const QString configText =
            takeCoreString(namaz_core_config_json(bootCore.handle()));
        const QJsonObject bootConfig =
            QJsonDocument::fromJson(configText.toUtf8()).object();
        locale = bootConfig.value(QStringLiteral("locale")).toString();
    }
    if (locale.isEmpty() || locale == QStringLiteral("system")) {
        locale = QLocale::system().name();
    }
    QLocale::setDefault(QLocale(locale));
    const QString languageCode = locale.section(QLatin1Char('_'), 0, 0);
    QTranslator *qtTranslator = new QTranslator(&app);
    if (languageCode != QStringLiteral("en")
        && qtTranslator->load(QLocale(locale), QStringLiteral("qtbase"),
                              QStringLiteral("_"),
                              QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(qtTranslator);
    }
    QTranslator *translator = new QTranslator(&app);
    if (languageCode != QStringLiteral("en")
        && translator->load(QStringLiteral(":/i18n/namaz_%1.qm").arg(languageCode))) {
        QApplication::installTranslator(translator);
    }

    QApplication::setWindowIcon(Ui::appIcon());

    QString dumpPath;
    QString pdfPath;
    QString screenshotPath;
    int printCalendar = 0;
    int initialTab = 0;
    bool tabRequested = false;
    bool openSettings = false;
    bool openPreferences = false;
    bool openMasjid = false;
    int settingsTab = 0;
    int preferencesTab = 0;
    QSize windowSize;
    const QStringList args = app.arguments();
    for (int index = 1; index < args.size(); ++index) {
        if (args[index] == QStringLiteral("--version")
            || args[index] == QStringLiteral("-v")) {
            QTextStream(stdout) << "Namaz " << QCoreApplication::applicationVersion()
                                << '\n';
            return 0;
        }
        if (args[index] == QStringLiteral("--help")
            || args[index] == QStringLiteral("-h")) {
            QTextStream(stdout)
                << "Usage: namaz [options]\n\n"
                << "  --tab 0|1|2            Open Day, Month or Year\n"
                << "  --settings             Open Prayer Settings\n"
                << "  --settings-tab N       Prayer Settings page\n"
                << "  --preferences          Open Preferences\n"
                << "  --preferences-tab N    Preferences page\n"
                << "  --masjid               Open the Masjid screen\n"
                << "  --window-size WxH      Force the main window size (debug)\n"
                << "  --print-pdf FILE       Print the month to PDF\n"
                << "  --print-calendar 0|1   0 = Gregorian, 1 = Hijri\n"
                << "  --dump-geometry FILE   Write the widget geometry as JSON\n"
                << "  --screenshot FILE      Grab the active window to PNG\n"
                << "  --version              Show the version\n";
            return 0;
        }
        if (args[index] == QStringLiteral("--dump-geometry") && index + 1 < args.size()) {
            dumpPath = args[++index];
        } else if (args[index] == QStringLiteral("--print-pdf") && index + 1 < args.size()) {
            pdfPath = args[++index];
        } else if (args[index] == QStringLiteral("--screenshot") && index + 1 < args.size()) {
            screenshotPath = args[++index];
        } else if (args[index] == QStringLiteral("--print-calendar") && index + 1 < args.size()) {
            printCalendar = args[++index].toInt() == 1 ? 1 : 0;
        } else if (args[index] == QStringLiteral("--tab") && index + 1 < args.size()) {
            initialTab = args[++index].toInt();
            tabRequested = true;
        } else if (args[index] == QStringLiteral("--settings")) {
            openSettings = true;
        } else if (args[index] == QStringLiteral("--settings-tab") && index + 1 < args.size()) {
            // The tab flag is enough on its own; the README documents it as a
            // standalone debug switch.
            settingsTab = args[++index].toInt();
            openSettings = true;
        } else if (args[index] == QStringLiteral("--preferences")) {
            openPreferences = true;
        } else if (args[index] == QStringLiteral("--masjid")) {
            openMasjid = true;
        } else if (args[index] == QStringLiteral("--preferences-tab") && index + 1 < args.size()) {
            preferencesTab = args[++index].toInt();
            openPreferences = true;
        } else if (args[index] == QStringLiteral("--window-size") && index + 1 < args.size()) {
            const QStringList parts = args[++index].split(QLatin1Char('x'));
            if (parts.size() == 2) {
                windowSize = QSize(parts.at(0).toInt(), parts.at(1).toInt());
            }
        }
    }

    const QString serverName =
        QStringLiteral("io.github.ikhlasulov_gb.Namaz-%1")
            .arg(qEnvironmentVariable("USER", QStringLiteral("user")));
    if (dumpPath.isEmpty() && pdfPath.isEmpty() && screenshotPath.isEmpty()) {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (socket.waitForConnected(300)) {
            socket.write(args.mid(1).join(QLatin1Char('\n')).toUtf8());
            socket.flush();
            socket.waitForBytesWritten(300);
            return 0;
        }
    }

    MainWindow window;
    if (windowSize.isValid()) {
        window.resize(windowSize);
    }
    // main.cpp must not override the tab restored from QSettings on every
    // start; only an explicit --tab should switch it.
    if (tabRequested) {
        window.setTab(initialTab);
    }

    if (!pdfPath.isEmpty()) {
        return window.printMonthToPath(pdfPath, printCalendar) ? 0 : 2;
    }

    QLocalServer *server = nullptr;
    if (dumpPath.isEmpty() && screenshotPath.isEmpty()) {
        QLocalServer::removeServer(serverName);
        server = new QLocalServer(&app);
        if (server->listen(serverName)) {
            QObject::connect(server, &QLocalServer::newConnection, &app,
                             [server, &window] {
                while (server->hasPendingConnections()) {
                    QLocalSocket *connection = server->nextPendingConnection();
                    auto handle = [connection, &window] {
                        if (connection->property("handled").toBool()) {
                            return;
                        }
                        connection->setProperty("handled", true);
                        const QString command =
                            QString::fromUtf8(connection->readAll());
                        const QStringList forwarded =
                            command.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                        int settingsTab = 0;
                        int preferencesTab = 0;
                        for (int i = 0; i + 1 < forwarded.size(); ++i) {
                            if (forwarded.at(i) == QStringLiteral("--tab")) {
                                window.setTab(forwarded.at(i + 1).toInt());
                            } else if (forwarded.at(i) == QStringLiteral("--settings-tab")) {
                                settingsTab = forwarded.at(i + 1).toInt();
                            } else if (forwarded.at(i)
                                       == QStringLiteral("--preferences-tab")) {
                                preferencesTab = forwarded.at(i + 1).toInt();
                            }
                        }
                        if (command.contains(QStringLiteral("--settings"))
                            || command.contains(QStringLiteral("--settings-tab"))) {
                            window.openSettingsDialog(settingsTab);
                        } else if (command.contains(QStringLiteral("--preferences"))
                                   || command.contains(
                                       QStringLiteral("--preferences-tab"))) {
                            window.openPreferencesDialog(preferencesTab);
                        } else if (command.contains(QStringLiteral("--masjid"))) {
                            window.showMasjid();
                        } else {
                            window.showFromTray();
                        }
                    };
                    QObject::connect(connection, &QLocalSocket::readyRead,
                                     connection, handle);
                    QObject::connect(connection, &QLocalSocket::disconnected,
                                     connection, &QObject::deleteLater);
                    if (connection->bytesAvailable() > 0) {
                        handle();
                    }
                }
            });
        }
    }

    QSplashScreen *splash = nullptr;
    if (dumpPath.isEmpty() && screenshotPath.isEmpty() && window.shouldShowSplash()) {
        splash = new QSplashScreen(window.windowIcon().pixmap(256));
        splash->show();
        app.processEvents();
    }

    window.show();
    window.raise();
    window.activateWindow();
    app.processEvents();

    if (splash) {
        QTimer::singleShot(1200, splash, [splash, &window] {
            splash->finish(&window);
            splash->deleteLater();
        });
    }

    if (window.startMinimized() && window.isTrayAvailable()) {
        window.showMinimized();
    }

    if (dumpPath.isEmpty() && screenshotPath.isEmpty()
        && !openSettings && !openPreferences) {
        window.playStartupSound();
    }

    if (openSettings) {
        window.openSettingsDialog(settingsTab);
        app.processEvents();
    }
    if (openPreferences) {
        window.openPreferencesDialog(preferencesTab);
        app.processEvents();
    }
    if (openMasjid) {
        window.showMasjid();
        app.processEvents();
    }

    if (!screenshotPath.isEmpty()) {
        app.processEvents();
        QWidget *target = QApplication::activeModalWidget();
        if (!target) {
            target = QApplication::activeWindow();
        }
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (widget != &window && widget->isVisible()) {
                target = widget;
            }
        }
        if (!target) {
            target = &window;
        }
        if (!target->grab().save(screenshotPath)) {
            return 2;
        }
        return 0;
    }

    if (!dumpPath.isEmpty()) {
        QWidget *origin = &window;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (widget != &window && widget->isVisible()) {
                origin = widget;
            }
        }
        QJsonObject root = dumpWidget(origin, origin);
        QJsonObject payload{
            {QStringLiteral("source"), QStringLiteral("namaz")},
            {QStringLiteral("coordinate_space"), QStringLiteral("top-level")},
            {QStringLiteral("root"), root},
        };
        QFile file(dumpPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return 2;
        }
        file.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
        return 0;
    }

    return app.exec();
}
