#include "Core.h"
#include "Dialogs.h"
#include "UiUtils.h"
#include "Views.h"
#include "Widgets.h"
#include "UpdateChecker.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QKeySequence>
#include <QSettings>
#include <QSignalSpy>
#include <QSlider>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

namespace {

QJsonObject configOf(Core &core) {
    const QString text = takeCoreString(namaz_core_config_json(core.handle()));
    return QJsonDocument::fromJson(text.toUtf8()).object();
}

bool applySettings(Core &core, const QJsonObject &object) {
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return namaz_core_apply_settings_json(core.handle(), json.constData()) != 0;
}

bool applyPreferences(Core &core, const QJsonObject &object) {
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return namaz_core_apply_preferences_json(core.handle(), json.constData()) != 0;
}

QPushButton *dialogButton(QDialog *dialog, QDialogButtonBox::StandardButton which) {
    auto *buttons = dialog->findChild<QDialogButtonBox *>(QStringLiteral("DialogButtons"));
    return buttons ? buttons->button(which) : nullptr;
}

}  // namespace

class DialogTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void appIconIsAlwaysAvailable();
    void dayViewQiblahArrowKeepsCompassDirection();
    void dayViewCardsShareRowGeometry();
    void dayViewWideLayout();
    void dayViewNarrowLayoutStacks();
    void dayViewArrowKeysNavigateDays();
    void yearViewClickOpensDay();
    void closeToTrayHidesWindow();
    void closeWithoutTrayClosesWindow();
    void windowGeometryRestores();
    void prayerSettingsPreserveManualCoordinates();
    void prayerSettingsReloadAfterCancel();
    void citySelectionResolvesTimezoneAndDst();
    void extremeComboMapsToItlConstants();
    void preferencesCancelDoesNotApplyVolume();
    void preferencesShuruqIsDisabled();
    void viewActionsStayExclusive();
    void windowRestoresSavedTab();
    void showFromTrayKeepsWindowState();
    void cityCountryMatchHandlesCompoundNames();
    void updateCheckerComparesVersions();
    void updateCheckerParsesRelease();
    void updateCheckerSelectsAsset();
    void updateCheckerFindsUpdateFromFileUrl();

private:
    QTemporaryDir m_home;
};

void DialogTest::initTestCase() {
    QVERIFY(m_home.isValid());
    qputenv("HOME", m_home.path().toUtf8());
}

void DialogTest::init() {
    // Every test starts from the built-in defaults.
    QFile::remove(m_home.path() + QStringLiteral("/.config/namaz/config.toml"));
}

void DialogTest::appIconIsAlwaysAvailable() {
    const QIcon icon = Ui::appIcon();
    QVERIFY(!icon.isNull());
    // A theme lookup may return a non-null icon without any pixmap.  The
    // window and tray need a real image, so insist on a usable fallback.
    QVERIFY(!icon.pixmap(32).isNull());
}

namespace {

QJsonObject dayState(bool clockwise) {
    QJsonArray times;
    for (int index = 0; index < 6; ++index) {
        times.append(QJsonObject{{QStringLiteral("seconds"), index * 3600 + 60}});
    }
    return QJsonObject{
        {QStringLiteral("date"),
         QJsonObject{{QStringLiteral("iso"), QStringLiteral("2026-11-08")},
                     {QStringLiteral("showHijri"), false}}},
        {QStringLiteral("now"),
         QJsonObject{{QStringLiteral("seconds"), 0}}},
        {QStringLiteral("next"),
         QJsonObject{{QStringLiteral("index"), 0},
                     {QStringLiteral("seconds"), 15480},
                     {QStringLiteral("remaining"), 15480},
                     {QStringLiteral("progress"), 1.0}}},
        {QStringLiteral("times"), times},
        {QStringLiteral("current"), QJsonObject{{QStringLiteral("index"), -1}}},
        {QStringLiteral("qibla"),
         QJsonObject{{QStringLiteral("bearing"), 294.526907},
                     {QStringLiteral("compass"), 294.526907},
                     {QStringLiteral("cardinal"), QStringLiteral("West 65.47° North")},
                     {QStringLiteral("clockwise"), clockwise}}},
        {QStringLiteral("showQiblah"), true},
    };
}

}  // namespace

void DialogTest::dayViewQiblahArrowKeepsCompassDirection() {
    DayView view;
    view.resize(920, 700);
    auto *compass =
        view.findChild<QiblahCompass *>(QStringLiteral("QiblahCompass"));
    auto *angle = view.findChild<QLabel *>(QStringLiteral("QiblahAngle"));
    auto *suffix = view.findChild<QLabel *>(QStringLiteral("QiblahSuffix"));
    QVERIFY(compass);
    QVERIFY(angle);
    QVERIFY(suffix);

    view.setState(dayState(false));
    QCOMPARE(qRound(compass->bearing() * 100.0) / 100.0, 294.53);
    QCOMPARE(angle->text(), QStringLiteral("West 65.47° North"));
    QCOMPARE(suffix->text(), QStringLiteral("from true North"));

    view.setState(dayState(true));
    QCOMPARE(qRound(compass->bearing() * 100.0) / 100.0, 294.53);
    QCOMPARE(angle->text(), QStringLiteral("294.53°"));
    QCOMPARE(suffix->text(), QStringLiteral("Clockwise from North"));
}

void DialogTest::dayViewCardsShareRowGeometry() {
    DayView view;
    view.resize(920, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTest::qWait(20);

    auto *current =
        view.findChild<QWidget *>(QStringLiteral("CurrentTimeCard"));
    auto *next = view.findChild<QWidget *>(QStringLiteral("NextPrayerCard"));
    auto *cards = view.findChild<QWidget *>(QStringLiteral("CardsRow"));
    QVERIFY(current);
    QVERIFY(next);
    QVERIFY(cards);
    QCOMPARE(current->y(), next->y());
    QCOMPARE(current->height(), next->height());
    QVERIFY(qAbs(current->width() - next->width()) <= 2);
    QVERIFY(next->x() >= current->x() + current->width());
}

void DialogTest::dayViewWideLayout() {
    DayView view;
    view.resize(920, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTest::qWait(20);

    auto *prayer =
        view.findChild<QWidget *>(QStringLiteral("PrayerTimesGroup"));
    auto *qiblah = view.findChild<QWidget *>(QStringLiteral("QiblahGroup"));
    QVERIFY(prayer);
    QVERIFY(qiblah);
    QCOMPARE(qiblah->y(), prayer->y());
    QCOMPARE(qiblah->height(), prayer->height());
    QVERIFY(qiblah->x() >= prayer->x() + prayer->width());
}

void DialogTest::dayViewNarrowLayoutStacks() {
    DayView view;
    view.resize(620, 900);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTest::qWait(20);

    auto *current =
        view.findChild<QWidget *>(QStringLiteral("CurrentTimeCard"));
    auto *next = view.findChild<QWidget *>(QStringLiteral("NextPrayerCard"));
    auto *prayer =
        view.findChild<QWidget *>(QStringLiteral("PrayerTimesGroup"));
    auto *qiblah = view.findChild<QWidget *>(QStringLiteral("QiblahGroup"));
    QVERIFY(current);
    QVERIFY(next);
    QVERIFY(prayer);
    QVERIFY(qiblah);
    QVERIFY(next->y() >= current->y() + current->height());
    QVERIFY(qiblah->y() >= prayer->y() + prayer->height());
    QCOMPARE(qiblah->x(), prayer->x());
}

void DialogTest::dayViewArrowKeysNavigateDays() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    auto *date = window.findChild<QLabel *>(QStringLiteral("DateLabel"));
    QVERIFY(date);
    const QString today = date->text();

    QTest::keyClick(&window, Qt::Key_Right);
    QTest::qWait(20);
    const QString next = date->text();
    QVERIFY2(next != today, "Right arrow did not move to the next day");

    QTest::keyClick(&window, Qt::Key_Left);
    QTest::qWait(20);
    QCOMPARE(date->text(), today);
}

void DialogTest::yearViewClickOpensDay() {
    YearView view;
    QJsonObject row{
        {QStringLiteral("date"), QStringLiteral("Nov 8")},
        {QStringLiteral("iso"), QStringLiteral("2026-11-08")},
        {QStringLiteral("current"), true},
    };
    QJsonObject year{
        {QStringLiteral("startIso"), QStringLiteral("2026-11-08")},
        {QStringLiteral("endIso"), QStringLiteral("2027-11-07")},
        {QStringLiteral("rows"), QJsonArray{row}},
    };
    view.setData(year);
    auto *table = view.findChild<QTableWidget *>(QStringLiteral("YearTable"));
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 1);

    view.resize(800, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QSignalSpy spy(&view, &YearView::openDayRequested);
    const QRect rect = table->visualItemRect(table->item(0, 0));
    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toDate(), QDate(2026, 11, 8));
}

void DialogTest::closeToTrayHidesWindow() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QSKIP("No system tray in this session");
    }
    {
        Core core;
        QVERIFY(applyPreferences(core, QJsonObject{{QStringLiteral("hide_on_close"), true}}));
    }
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.isVisible());
    window.close();
    QTest::qWait(100);
    QVERIFY(!window.isVisible());
}

void DialogTest::closeWithoutTrayClosesWindow() {
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        QSKIP("System tray is present");
    }
    {
        Core core;
        QVERIFY(applyPreferences(core, QJsonObject{{QStringLiteral("hide_on_close"), true}}));
    }
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.isVisible());
    QVERIFY(window.close());
    QTest::qWait(100);
    QVERIFY(!window.isVisible());
}

void DialogTest::windowGeometryRestores() {
    QSettings settings;
    settings.clear();
    QWidget probe;
    probe.resize(720, 540);
    settings.setValue(QStringLiteral("window/layoutVersion"), 3);
    settings.setValue(QStringLiteral("window/geometry"), probe.saveGeometry());
    settings.setValue(QStringLiteral("window/state"), QByteArray());

    MainWindow window;
    QCOMPARE(window.size(), QSize(720, 540));
    settings.clear();
}

void DialogTest::prayerSettingsPreserveManualCoordinates() {
    Core core;
    QJsonObject settings{
        {QStringLiteral("city"), QStringLiteral("Palembang | (ID25), Indonesia")},
        {QStringLiteral("province"), QString()},
        {QStringLiteral("latitude"), -2.9},
        {QStringLiteral("longitude"), 104.7},
        {QStringLiteral("timezone"), 7.0},
        {QStringLiteral("timezone_name"), QString()},
        {QStringLiteral("dst_mode"), 0},
    };
    QVERIFY(applySettings(core, settings));

    PrayerSettingsDialog dialog(core.handle());
    auto *province = dialog.findChild<QLineEdit *>(QStringLiteral("ProvinceEdit"));
    auto *latitude = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("LatitudeSpin"));
    auto *longitude = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("LongitudeSpin"));
    QVERIFY(province);
    QVERIFY(latitude);
    QVERIFY(longitude);

    // Opening the dialog must not copy the first database city over manual
    // coordinates or fill Province behind the user's back.
    QCOMPARE(province->text(), QString());
    QVERIFY(qAbs(latitude->value() - (-2.9)) < 1e-6);
    QVERIFY(qAbs(longitude->value() - 104.7) < 1e-6);

    auto *apply = dialogButton(&dialog, QDialogButtonBox::Apply);
    QVERIFY(apply);
    apply->click();

    const QJsonObject after = configOf(core);
    QCOMPARE(after.value(QStringLiteral("province")).toString(), QString());
    QVERIFY(qAbs(after.value(QStringLiteral("latitude")).toDouble() - (-2.9)) < 1e-6);
    QVERIFY(qAbs(after.value(QStringLiteral("longitude")).toDouble() - 104.7) < 1e-6);
}

void DialogTest::prayerSettingsReloadAfterCancel() {
    Core core;
    QJsonObject settings{
        {QStringLiteral("city"), QStringLiteral("Custom, Test")},
        {QStringLiteral("latitude"), -2.9},
        {QStringLiteral("longitude"), 104.7},
        {QStringLiteral("timezone"), 7.0},
        {QStringLiteral("timezone_name"), QString()},
        {QStringLiteral("dst_mode"), 0},
    };
    QVERIFY(applySettings(core, settings));

    PrayerSettingsDialog dialog(core.handle());
    auto *latitude = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("LatitudeSpin"));
    QVERIFY(latitude);
    latitude->setValue(12.0);

    auto *cancel = dialogButton(&dialog, QDialogButtonBox::Cancel);
    QVERIFY(cancel);
    cancel->click();

    dialog.reload();
    QVERIFY(qAbs(latitude->value() - (-2.9)) < 1e-6);
}

void DialogTest::citySelectionResolvesTimezoneAndDst() {
    Core core;
    PrayerSettingsDialog dialog(core.handle());
    auto *countries = dialog.findChild<QListWidget *>(QStringLiteral("CountryList"));
    auto *cities = dialog.findChild<QListWidget *>(QStringLiteral("CityList"));
    QVERIFY(countries);
    QVERIFY(cities);

    int unitedKingdom = -1;
    for (int row = 0; row < countries->count(); ++row) {
        if (countries->item(row)->text() == QStringLiteral("United Kingdom")) {
            unitedKingdom = row;
            break;
        }
    }
    QVERIFY(unitedKingdom >= 0);
    countries->setCurrentRow(unitedKingdom);

    int london = -1;
    for (int row = 0; row < cities->count(); ++row) {
        if (cities->item(row)->text().startsWith(QStringLiteral("London"))) {
            london = row;
            break;
        }
    }
    QVERIFY(london >= 0);
    cities->setCurrentRow(london);

    auto *zone = dialog.findChild<QComboBox *>(QStringLiteral("TimezoneCombo"));
    auto *dst = dialog.findChild<QComboBox *>(QStringLiteral("DstModeCombo"));
    auto *latitude = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("LatitudeSpin"));
    QVERIFY(zone);
    QVERIFY(dst);
    QVERIFY(latitude);

    QCOMPARE(zone->currentData().toString(), QStringLiteral("Europe/London"));
    QCOMPARE(dst->currentData().toInt(), 1);
    QVERIFY(qAbs(latitude->value() - 51.5074) < 0.02);

    auto *apply = dialogButton(&dialog, QDialogButtonBox::Apply);
    QVERIFY(apply);
    apply->click();
    const QJsonObject cfg = configOf(core);
    QCOMPARE(cfg.value(QStringLiteral("timezone_name")).toString(),
             QStringLiteral("Europe/London"));
    QCOMPARE(cfg.value(QStringLiteral("dst_mode")).toInt(), 1);
    QVERIFY(qAbs(cfg.value(QStringLiteral("timezone")).toDouble()) < 1e-6);
}

void DialogTest::extremeComboMapsToItlConstants() {
    Core core;
    PrayerSettingsDialog dialog(core.handle());
    auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("ExtremeCombo"));
    auto *apply = dialogButton(&dialog, QDialogButtonBox::Apply);
    QVERIFY(combo);
    QVERIFY(apply);

    const int expected[] = {0, 10, 11, 6, 7, 5};
    for (int index = 0; index < 6; ++index) {
        combo->setCurrentIndex(index);
        apply->click();
        QCOMPARE(configOf(core).value(QStringLiteral("extreme")).toInt(), expected[index]);
    }
}

void DialogTest::preferencesCancelDoesNotApplyVolume() {
    Core core;
    QVERIFY(applyPreferences(core, QJsonObject{{QStringLiteral("volume"), 55}}));
    QCOMPARE(namaz_core_volume(), 55);

    PreferencesDialog dialog(core.handle());
    auto *slider = dialog.findChild<QSlider *>(QStringLiteral("VolumeSlider"));
    QVERIFY(slider);
    QCOMPARE(slider->value(), 55);

    slider->setValue(80);
    QCOMPARE(namaz_core_volume(), 55);

    auto *cancel = dialogButton(&dialog, QDialogButtonBox::Cancel);
    QVERIFY(cancel);
    cancel->click();
    QCOMPARE(namaz_core_volume(), 55);

    dialog.reload();
    QCOMPARE(slider->value(), 55);

    slider->setValue(80);
    auto *apply = dialogButton(&dialog, QDialogButtonBox::Apply);
    QVERIFY(apply);
    apply->click();
    QCOMPARE(namaz_core_volume(), 80);
    QCOMPARE(configOf(core).value(QStringLiteral("volume")).toInt(), 80);
}

void DialogTest::preferencesShuruqIsDisabled() {
    Core core;
    PreferencesDialog dialog(core.handle());
    auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("AthanMode1"));
    auto *iqama = dialog.findChild<QCheckBox *>(QStringLiteral("IqamaOn1"));
    QVERIFY(mode);
    QVERIFY(iqama);
    QVERIFY(!mode->isEnabled());
    QVERIFY(!iqama->isEnabled());
    QVERIFY(!iqama->isChecked());
    QCOMPARE(mode->currentData().toInt(), 0);
}

namespace {

int checkedViewActions(MainWindow &window) {
    int checked = 0;
    const QList<QKeySequence> viewShortcuts{
        QKeySequence(QStringLiteral("Ctrl+1")),
        QKeySequence(QStringLiteral("Ctrl+2")),
        QKeySequence(QStringLiteral("Ctrl+3")),
    };
    for (QAction *action : window.findChildren<QAction *>()) {
        if (viewShortcuts.contains(action->shortcut()) && action->isChecked()) {
            ++checked;
        }
    }
    return checked;
}

}  // namespace

void DialogTest::viewActionsStayExclusive() {
    QSettings settings;
    settings.clear();
    MainWindow window;
    window.setTab(2);
    QCOMPARE(checkedViewActions(window), 1);
    window.setTab(0);
    QCOMPARE(checkedViewActions(window), 1);
    window.setTab(1);
    QCOMPARE(checkedViewActions(window), 1);
    settings.clear();
}

void DialogTest::windowRestoresSavedTab() {
    QSettings settings;
    settings.clear();
    settings.setValue(QStringLiteral("window/tab"), 2);
    MainWindow window;
    auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("ViewStack"));
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), 2);
    QCOMPARE(checkedViewActions(window), 1);
    settings.clear();
}

void DialogTest::showFromTrayKeepsWindowState() {
    MainWindow window;
    window.showMaximized();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.isMaximized());
    window.hide();
    window.showFromTray();
    QVERIFY(window.isVisible());
    QVERIFY(window.isMaximized());
    window.close();
}

void DialogTest::cityCountryMatchHandlesCompoundNames() {
    Core core;
    QJsonObject settings{
        {QStringLiteral("city"),
         QStringLiteral("Port Moresby | National Capital, Papua New Guinea")},
    };
    QVERIFY(applySettings(core, settings));
    PrayerSettingsDialog dialog(core.handle());
    auto *countries = dialog.findChild<QListWidget *>(QStringLiteral("CountryList"));
    QVERIFY(countries);
    QVERIFY(countries->currentItem());
    // "Papua New Guinea" must not be mistaken for the shorter "Guinea".
    QCOMPARE(countries->currentItem()->text(), QStringLiteral("Papua New Guinea"));
}

void DialogTest::updateCheckerComparesVersions() {
    QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.0.0"),
                                            QStringLiteral("1.0.0")), 0);
    QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("v1.0"),
                                            QStringLiteral("1.0.0")), 0);
    QVERIFY(UpdateChecker::compareVersions(QStringLiteral("1.0.1"),
                                           QStringLiteral("1.0.0")) > 0);
    QVERIFY(UpdateChecker::compareVersions(QStringLiteral("v1.2.0"),
                                           QStringLiteral("1.1.9")) > 0);
    QVERIFY(UpdateChecker::compareVersions(QStringLiteral("1.0.0-beta"),
                                           QStringLiteral("1.0.0")) < 0);
    QVERIFY(UpdateChecker::compareVersions(QStringLiteral("1.0.0"),
                                           QStringLiteral("1.0.0-beta")) > 0);
}

void DialogTest::updateCheckerParsesRelease() {
    const QByteArray payload = R"({
        "tag_name": "v1.2.3",
        "html_url": "https://github.com/ikhlasulov-gb/namaz/releases/tag/v1.2.3",
        "body": "Release notes",
        "assets": [
            {"name": "Namaz-1.2.3-x86_64.AppImage",
             "browser_download_url": "https://example.invalid/namaz.AppImage"},
            {"name": "SHA256SUMS",
             "browser_download_url": "https://example.invalid/SHA256SUMS"}
        ]
    })";
    UpdateChecker::Release release;
    QString error;
    QVERIFY2(UpdateChecker::parseRelease(payload, &release, &error),
             qPrintable(error));
    QCOMPARE(release.version, QStringLiteral("1.2.3"));
    QCOMPARE(release.tagName, QStringLiteral("v1.2.3"));
    QCOMPARE(release.body, QStringLiteral("Release notes"));
    QCOMPARE(release.assets.size(), 2);
    QCOMPARE(release.assetUrl(QStringLiteral("SHA256SUMS")),
             QStringLiteral("https://example.invalid/SHA256SUMS"));

    QVERIFY(!UpdateChecker::parseRelease(
        QByteArrayLiteral(R"({"message": "Not Found"})"), &release, &error));
    QVERIFY(!error.isEmpty());
}

void DialogTest::updateCheckerSelectsAsset() {
    UpdateChecker::Release release;
    release.assets = {
        {QStringLiteral("Namaz-1.2.3-x86_64.AppImage"),
         QStringLiteral("https://example.invalid/x86_64.AppImage")},
        {QStringLiteral("Namaz-1.2.3-aarch64.AppImage"),
         QStringLiteral("https://example.invalid/aarch64.AppImage")},
    };
    const QString url =
        UpdateChecker::releaseAssetUrl(release, QStringLiteral(".AppImage"));
    QVERIFY(url.endsWith(QStringLiteral(".AppImage")));
    QVERIFY(url.contains(UpdateChecker::currentArchitecture()));
}

void DialogTest::updateCheckerFindsUpdateFromFileUrl() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("release.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"({
        "tag_name": "v1.2.0",
        "html_url": "https://example.invalid/release",
        "body": "Notes",
        "assets": []
    })");
    file.close();

    const QString previousVersion = QCoreApplication::applicationVersion();
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QSettings().clear();
    qputenv("NAMAZ_UPDATE_URL", QUrl::fromLocalFile(path).toEncoded());

    UpdateChecker checker;
    QSignalSpy spy(&checker, &UpdateChecker::updateAvailable);
    checker.check(true);
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);

    QVERIFY(!spy.takeFirst().isEmpty());
    qunsetenv("NAMAZ_UPDATE_URL");
    QCoreApplication::setApplicationVersion(previousVersion);
    QSettings().clear();
}

QTEST_MAIN(DialogTest)

#include "tst_dialogs.moc"
