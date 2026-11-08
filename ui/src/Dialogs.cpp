#include "Dialogs.h"

#include "Core.h"
#include "UiUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTime>
#include <QTimeEdit>
#include <QTimeZone>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int CityNameRole = Qt::UserRole;
constexpr int CityStateRole = Qt::UserRole + 1;
constexpr int CityLatRole = Qt::UserRole + 2;
constexpr int CityLonRole = Qt::UserRole + 3;
constexpr int CityTzRole = Qt::UserRole + 4;
constexpr int CityTzNameRole = Qt::UserRole + 5;

QJsonObject configObject(void *core) {
    const QString text = takeCoreString(namaz_core_config_json(core));
    return QJsonDocument::fromJson(text.toUtf8()).object();
}

QJsonObject defaultConfigObject() {
    const QString text = takeCoreString(namaz_core_default_config_json());
    return QJsonDocument::fromJson(text.toUtf8()).object();
}

QStringList availableAthanSounds() {
    const QString text = takeCoreString(namaz_core_athan_sounds_json());
    QStringList result;
    for (const auto &value : QJsonDocument::fromJson(text.toUtf8()).array()) {
        result.append(value.toString());
    }
    return result;
}

QDoubleSpinBox *doubleSpin(QWidget *parent, double minimum, double maximum,
                           int decimals, double step) {
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setDecimals(decimals);
    spin->setSingleStep(step);
    return spin;
}

QComboBox *soundCombo(QWidget *parent, const QStringList &sounds,
                      const QString &selected) {
    auto *combo = new QComboBox(parent);
    combo->addItem(QObject::tr("Default athan"), QString());
    combo->addItem(QObject::tr("<Random>"), QStringLiteral("<random>"));
    for (const QString &sound : sounds) {
        combo->addItem(sound, sound);
    }
    int index = combo->findData(selected);
    if (index < 0 && !selected.isEmpty()) {
        combo->addItem(selected, selected);
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(qMax(0, index));
    return combo;
}

QToolButton *smallButton(QWidget *parent, const QString &icon,
                         const QString &tooltip) {
    auto *button = new QToolButton(parent);
    button->setAutoRaise(true);
    button->setIcon(Ui::themeIcon(icon));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setIconSize(QSize(16, 16));
    return button;
}

QSpinBox *minutesSpin(QWidget *parent) {
    auto *spin = new QSpinBox(parent);
    spin->setRange(0, 120);
    spin->setSuffix(QObject::tr(" min"));
    return spin;
}

struct Page {
    QWidget *widget = nullptr;
    QVBoxLayout *layout = nullptr;
};

struct Navigation {
    QListWidget *list = nullptr;
    QStackedWidget *stack = nullptr;
};

Navigation createNavigation(QWidget *parent, QVBoxLayout *outer,
                            const QString &objectPrefix) {
    auto *list = new QListWidget(parent);
    list->setObjectName(objectPrefix + QStringLiteral("Nav"));
    list->setFrameShape(QFrame::NoFrame);
    list->setIconSize(QSize(16, 16));
    list->setUniformItemSizes(true);
    list->setSpacing(1);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setMinimumWidth(140);
    list->setMaximumWidth(180);
    list->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto *stack = new QStackedWidget(parent);
    stack->setObjectName(objectPrefix + QStringLiteral("Stack"));

    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(Ui::sectionSpacing());
    row->addWidget(list);
    row->addWidget(stack, 1);
    outer->addLayout(row, 1);

    QObject::connect(list, &QListWidget::currentRowChanged, stack,
                     &QStackedWidget::setCurrentIndex);
    return {list, stack};
}

Page addScrollPage(Navigation &nav, const QString &title, const QString &icon) {
    auto *scroll = new QScrollArea(nav.stack);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(Ui::groupSpacing() * 2);
    scroll->setWidget(widget);
    nav.stack->addWidget(scroll);

    auto *item = new QListWidgetItem(Ui::themeIcon(icon), title);
    item->setSizeHint(QSize(0, 30));
    nav.list->addItem(item);
    return {widget, layout};
}

QGroupBox *compactBox(const QString &title, QWidget *parent) {
    auto *box = new QGroupBox(title, parent);
    box->setAlignment(Qt::AlignLeft);
    return box;
}

QFormLayout *compactForm(QGroupBox *box) {
    auto *form = new QFormLayout(box);
    Ui::compactForm(form);
    return form;
}

QWidget *volumeRow(QWidget *parent, QSlider *&slider, QLabel *&value) {
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Ui::controlSpacing());
    layout->addWidget(new QLabel(QObject::tr("Volume"), row));
    slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, 100);
    slider->setValue(90);
    slider->setObjectName(QStringLiteral("VolumeSlider"));
    slider->setMinimumWidth(160);
    slider->setMaximumWidth(200);
    value = new QLabel(QStringLiteral("90%"), row);
    value->setMinimumWidth(42);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(slider);
    layout->addWidget(value);
    layout->addStretch(1);
    return row;
}

QLabel *descriptionLabel(QWidget *parent) {
    auto *label = new QLabel(parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

// Size a settings dialog so the tallest page fits without a vertical
// scrollbar.  The dialogs keep their scroll areas for small screens, but on a
// normal desktop screen everything is visible at once.
void fitDialogToPages(QDialog *dialog, QStackedWidget *stack,
                      QDialogButtonBox *buttons, int preferredWidth) {
    if (!dialog || !stack) {
        return;
    }
    int pageWidth = 0;
    int pageHeight = 0;
    for (int index = 0; index < stack->count(); ++index) {
        auto *scroll = qobject_cast<QScrollArea *>(stack->widget(index));
        if (!scroll || !scroll->widget()) {
            continue;
        }
        const QSize hint = scroll->widget()->sizeHint();
        pageWidth = qMax(pageWidth, hint.width());
        pageHeight = qMax(pageHeight, hint.height());
    }
    if (pageHeight <= 0) {
        return;
    }
    int height = pageHeight + 16;
    if (QLayout *layout = dialog->layout()) {
        const QMargins margins = layout->contentsMargins();
        height += margins.top() + margins.bottom() + layout->spacing();
    }
    if (buttons) {
        height += buttons->sizeHint().height();
    }
    QSize available(1920, 1200);
    QScreen *screen = dialog->screen();
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen) {
        available = screen->availableGeometry().size();
    }
    const int width = qMin(qMax(preferredWidth, pageWidth + 220),
                           available.width() - 80);
    dialog->resize(width, qMin(height, available.height() - 80));
}

}  // namespace

PrayerSettingsDialog::PrayerSettingsDialog(void *core, QWidget *parent)
    : QDialog(parent), m_core(core) {
    setWindowTitle(tr("Namaz — Prayer Settings"));
    setModal(true);
    setMinimumSize(640, 440);
    resize(800, 700);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(Ui::groupSpacing());

    Navigation nav = createNavigation(this, layout, QStringLiteral("PrayerSettings"));
    m_nav = nav.list;
    m_stack = nav.stack;

    Page locationPage = addScrollPage(nav, tr("Location"), QStringLiteral("mark-location"));
    auto *locationLayout = locationPage.layout;

    auto *listBox = compactBox(tr("Select Location From List"), locationPage.widget);
    auto *listLayout = new QHBoxLayout(listBox);
    listLayout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                                   Ui::pageMargin(), Ui::pageMargin());
    listLayout->setSpacing(Ui::controlSpacing());

    auto *countryColumn = new QWidget(listBox);
    auto *countryLayout = new QVBoxLayout(countryColumn);
    countryLayout->setContentsMargins(0, 0, 0, 0);
    countryLayout->setSpacing(Ui::controlSpacing());
    countryLayout->addWidget(new QLabel(tr("Countries"), countryColumn));
    m_countries = new QListWidget(countryColumn);
    m_countries->setObjectName(QStringLiteral("CountryList"));
    m_countries->setAlternatingRowColors(true);
    m_countries->setTextElideMode(Qt::ElideRight);
    m_countries->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_countries->setMinimumHeight(110);
    m_countries->setMaximumHeight(140);
    countryLayout->addWidget(m_countries, 1);

    auto *cityColumn = new QWidget(listBox);
    auto *cityLayout = new QVBoxLayout(cityColumn);
    cityLayout->setContentsMargins(0, 0, 0, 0);
    cityLayout->setSpacing(Ui::controlSpacing());
    cityLayout->addWidget(new QLabel(tr("Cities"), cityColumn));
    m_cities = new QListWidget(cityColumn);
    m_cities->setObjectName(QStringLiteral("CityList"));
    m_cities->setAlternatingRowColors(true);
    m_cities->setTextElideMode(Qt::ElideRight);
    m_cities->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cities->setMinimumHeight(110);
    m_cities->setMaximumHeight(140);
    cityLayout->addWidget(m_cities, 1);

    listLayout->addWidget(countryColumn, 1);
    listLayout->addWidget(cityColumn, 1);
    locationLayout->addWidget(listBox);

    auto *manualBox = compactBox(tr("Current Location / Manual Adjust"),
                                 locationPage.widget);
    auto *locationForm = compactForm(manualBox);

    m_city = new QLineEdit(manualBox);
    m_city->setObjectName(QStringLiteral("CityEdit"));
    m_province = new QLineEdit(manualBox);
    m_province->setObjectName(QStringLiteral("ProvinceEdit"));
    m_latitude = doubleSpin(manualBox, -90.0, 90.0, 4, 0.0001);
    m_latitude->setObjectName(QStringLiteral("LatitudeSpin"));
    m_longitude = doubleSpin(manualBox, -180.0, 180.0, 4, 0.0001);
    m_longitude->setObjectName(QStringLiteral("LongitudeSpin"));
    // Quarter-hour zones (e.g. Nepal 5.75, Chatham 12.75) must survive.
    m_timezone = doubleSpin(manualBox, -12.0, 14.0, 2, 0.5);
    m_timezone->setObjectName(QStringLiteral("TimezoneSpin"));
    m_timeZoneName = new QComboBox(manualBox);
    m_timeZoneName->setObjectName(QStringLiteral("TimezoneCombo"));
    m_timeZoneName->setEditable(false);
    m_timeZoneName->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_timeZoneName->setMinimumContentsLength(24);
    populateTimeZones();

    m_dstMode = new QComboBox(manualBox);
    m_dstMode->setObjectName(QStringLiteral("DstModeCombo"));
    m_dstMode->addItem(tr("No daylight saving"), 0);
    m_dstMode->addItem(tr("Automatic (use IANA time zone)"), 1);
    m_dstMode->addItem(tr("Manual +1 hour"), 2);

    Ui::setFieldWidth(m_city, 320);
    Ui::setFieldWidth(m_province, 280);
    Ui::setFieldWidth(m_latitude, 140);
    Ui::setFieldWidth(m_longitude, 140);
    Ui::setFieldWidth(m_timeZoneName, 280);
    Ui::setFieldWidth(m_timezone, 120);
    Ui::setFieldWidth(m_dstMode, 300);

    locationForm->addRow(tr("City"), m_city);
    locationForm->addRow(tr("Province / State"), m_province);
    locationForm->addRow(tr("Latitude [-90, +90]"), m_latitude);
    locationForm->addRow(tr("Longitude [-180, +180]"), m_longitude);
    locationForm->addRow(tr("Time zone"), m_timeZoneName);
    locationForm->addRow(tr("UTC offset"), m_timezone);
    locationForm->addRow(tr("Daylight saving"), m_dstMode);
    locationLayout->addWidget(manualBox);

    auto *timeNote = descriptionLabel(locationPage.widget);
    timeNote->setText(tr("Automatic daylight saving uses the selected IANA time zone. "
                         "With an empty time zone the numeric UTC offset is used as-is."));
    locationLayout->addWidget(timeNote);

    connect(m_countries, &QListWidget::currentRowChanged, this, [this](int) {
        if (m_updating || m_populating) {
            return;
        }
        if (auto *item = m_countries->currentItem()) {
            // Loading a country only fills the city list; the current
            // location changes when the user actually picks a city.
            loadCities(item->text(), false);
        }
    });
    connect(m_cities, &QListWidget::currentRowChanged, this, [this](int) {
        if (!m_updating && !m_populating) {
            selectCity();
        }
    });
    connect(m_cities, &QListWidget::itemClicked, this, [this](QListWidgetItem *) {
        if (!m_updating && !m_populating) {
            selectCity();
        }
    });
    connect(m_timeZoneName, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_updating) {
            return;
        }
        const QString id = m_timeZoneName->currentData().toString();
        if (id.isEmpty()) {
            const int none = m_dstMode->findData(0);
            if (none >= 0) {
                m_dstMode->setCurrentIndex(none);
            }
            return;
        }
        const QTimeZone zone(id.toUtf8());
        if (zone.isValid()) {
            // Keep the numeric field at standard time; DST is applied by the
            // IANA zone in automatic mode.
            m_timezone->setValue(
                zone.standardTimeOffset(QDateTime::currentDateTime()) / 3600.0);
            if (zone.hasDaylightTime()) {
                const int automatic = m_dstMode->findData(1);
                if (automatic >= 0) {
                    m_dstMode->setCurrentIndex(automatic);
                }
            }
        }
    });
    Page calculationPage = addScrollPage(nav, tr("Calculation Method"),
                                         QStringLiteral("preferences-system-time"));
    auto *calculationLayout = calculationPage.layout;

    auto *methodBox = compactBox(tr("Calculation Method"), calculationPage.widget);
    auto *methodForm = compactForm(methodBox);

    m_method = new QComboBox(methodBox);
    m_method->setObjectName(QStringLiteral("MethodCombo"));
    m_method->addItem(tr("Custom twilight angles"), 0);
    m_method->addItem(tr("Egyptian General Authority of Survey"), 1);
    m_method->addItem(tr("Islamic Society of North America (ISNA)"), 4);
    m_method->addItem(tr("Muslim World League"), 5);
    m_method->addItem(tr("Umm al-Qura"), 6);
    m_method->addItem(tr("University of Islamic Sciences, Karachi"), 2);
    Ui::setFieldWidth(m_method, 340);
    methodForm->addRow(tr("Method"), m_method);
    m_methodDescription = descriptionLabel(methodBox);
    methodForm->addRow(QString(), m_methodDescription);

    auto *angles = new QWidget(methodBox);
    auto *anglesLayout = new QHBoxLayout(angles);
    anglesLayout->setContentsMargins(0, 0, 0, 0);
    anglesLayout->setSpacing(Ui::controlSpacing());
    m_fajrAngle = doubleSpin(angles, 0.0, 30.0, 1, 0.5);
    m_fajrAngle->setObjectName(QStringLiteral("FajrAngleSpin"));
    m_ishaaAngle = doubleSpin(angles, 0.0, 30.0, 1, 0.5);
    m_ishaaAngle->setObjectName(QStringLiteral("IshaaAngleSpin"));
    Ui::setFieldWidth(m_fajrAngle, 110);
    Ui::setFieldWidth(m_ishaaAngle, 110);
    anglesLayout->addWidget(new QLabel(tr("Fajr"), angles));
    anglesLayout->addWidget(m_fajrAngle);
    anglesLayout->addSpacing(Ui::sectionSpacing());
    anglesLayout->addWidget(new QLabel(tr("Ishaa"), angles));
    anglesLayout->addWidget(m_ishaaAngle);
    anglesLayout->addStretch(1);
    methodForm->addRow(tr("Twilight angles"), angles);

    m_mathhab = new QComboBox(methodBox);
    m_mathhab->setObjectName(QStringLiteral("MathhabCombo"));
    m_mathhab->addItem(tr("Standard (Shafi'i, Hanbali, Maliki)"), 1);
    m_mathhab->addItem(tr("Hanafi"), 2);
    Ui::setFieldWidth(m_mathhab, 340);
    methodForm->addRow(tr("Asr juristic method"), m_mathhab);

    calculationLayout->addWidget(methodBox);

    auto *timesBox = compactBox(tr("Calculated time adjustments"),
                                calculationPage.widget);
    auto *timesForm = compactForm(timesBox);

    m_dhuhrMinutes = new QSpinBox(timesBox);
    m_dhuhrMinutes->setObjectName(QStringLiteral("DhuhrMinutesSpin"));
    m_dhuhrMinutes->setRange(0, 60);
    m_maghribMinutes = new QSpinBox(timesBox);
    m_maghribMinutes->setObjectName(QStringLiteral("MaghribMinutesSpin"));
    m_maghribMinutes->setRange(0, 60);
    m_rounding = new QComboBox(timesBox);
    m_rounding->setObjectName(QStringLiteral("RoundingCombo"));
    m_rounding->addItem(tr("No rounding"), 0);
    m_rounding->addItem(tr("Normal rounding"), 1);
    m_rounding->addItem(tr("Special rounding"), 2);
    m_rounding->addItem(tr("Aggressive rounding"), 3);
    m_extreme = new QComboBox(timesBox);
    m_extreme->setObjectName(QStringLiteral("ExtremeCombo"));
    m_extreme->addItem(tr("None"), 0);
    m_extreme->addItem(tr("Middle of the night — always"), 10);
    m_extreme->addItem(tr("Middle of the night — invalid only"), 11);
    m_extreme->addItem(tr("One-seventh of the night — always"), 6);
    m_extreme->addItem(tr("One-seventh of the night — invalid only"), 7);
    m_extreme->addItem(tr("Nearest good day (Aqrab Al-Ayyam)"), 5);
    m_extremeDescription = descriptionLabel(timesBox);

    Ui::setFieldWidth(m_dhuhrMinutes, 110);
    Ui::setFieldWidth(m_maghribMinutes, 110);
    Ui::setFieldWidth(m_rounding, 280);
    Ui::setFieldWidth(m_extreme, 340);

    timesForm->addRow(tr("Dhuhr minutes after Zawal"), m_dhuhrMinutes);
    timesForm->addRow(tr("Maghrib minutes after sunset"), m_maghribMinutes);
    timesForm->addRow(tr("Seconds rounding"), m_rounding);
    timesForm->addRow(tr("Extreme latitude method"), m_extreme);
    timesForm->addRow(QString(), m_extremeDescription);
    calculationLayout->addWidget(timesBox);

    auto *adjustBox = compactBox(tr("Additional adjustment (minutes)"),
                                 calculationPage.widget);
    auto *adjustLayout = new QGridLayout(adjustBox);
    Ui::compactGrid(adjustLayout);
    const QString prayerNames[] = {tr("Fajr"), tr("Shuruq"), tr("Dhuhr"),
                                   tr("Asr"), tr("Maghrib"), tr("Ishaa")};
    for (int index = 0; index < 6; ++index) {
        adjustLayout->addWidget(new QLabel(prayerNames[index], adjustBox), 0, index);
        m_adjust[index] = new QSpinBox(adjustBox);
        m_adjust[index]->setRange(-60, 60);
        m_adjust[index]->setSuffix(tr(" min"));
        Ui::setFieldWidth(m_adjust[index], 96);
        adjustLayout->addWidget(m_adjust[index], 1, index);
    }
    calculationLayout->addWidget(adjustBox);
    calculationLayout->addStretch(1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        this);
    buttons->setObjectName(QStringLiteral("DialogButtons"));
    m_applyButton = buttons->button(QDialogButtonBox::Apply);
    if (m_applyButton) {
        m_applyButton->setEnabled(false);
    }
    QPushButton *restore = buttons->addButton(
        tr("Restore Defaults"), QDialogButtonBox::ResetRole);
    connect(restore, &QPushButton::clicked, this, [this] {
        populate(defaultConfigObject());
        // The dialog now differs from the saved configuration, so the
        // defaults can be applied.
        if (m_applyButton) {
            m_applyButton->setEnabled(true);
        }
    });
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            [this] {
                if (validate()) {
                    apply();
                }
            });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (validate() && apply()) {
            accept();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_method, &QComboBox::currentIndexChanged, this,
            &PrayerSettingsDialog::updateMethodDescription);
    connect(m_extreme, &QComboBox::currentIndexChanged, this,
            &PrayerSettingsDialog::updateExtremeDescription);

    connectChangeTracking();
    populate(configObject(m_core));
    fitDialogToPages(this, m_stack, buttons, 820);
}

void PrayerSettingsDialog::populateTimeZones() {
    m_timeZoneName->clear();
    m_timeZoneName->addItem(tr("Use numeric UTC offset only"), QString());
    QStringList ids;
    const QList<QByteArray> rawIds = QTimeZone::availableTimeZoneIds();
    ids.reserve(rawIds.size());
    for (const QByteArray &id : rawIds) {
        ids.append(QString::fromUtf8(id));
    }
    ids.sort(Qt::CaseInsensitive);
    for (const QString &id : ids) {
        m_timeZoneName->addItem(id, id);
    }
}

void PrayerSettingsDialog::markDirty() {
    if (m_updating || m_populating) {
        return;
    }
    if (m_applyButton) {
        m_applyButton->setEnabled(true);
    }
}

void PrayerSettingsDialog::connectChangeTracking() {
    for (QLineEdit *edit : findChildren<QLineEdit *>()) {
        connect(edit, &QLineEdit::textEdited, this,
                &PrayerSettingsDialog::markDirty);
    }
    for (QComboBox *combo : findChildren<QComboBox *>()) {
        connect(combo, &QComboBox::currentIndexChanged, this,
                &PrayerSettingsDialog::markDirty);
    }
    for (QSpinBox *spin : findChildren<QSpinBox *>()) {
        connect(spin, &QSpinBox::valueChanged, this,
                &PrayerSettingsDialog::markDirty);
    }
    for (QDoubleSpinBox *spin : findChildren<QDoubleSpinBox *>()) {
        connect(spin, &QDoubleSpinBox::valueChanged, this,
                &PrayerSettingsDialog::markDirty);
    }
    for (QCheckBox *check : findChildren<QCheckBox *>()) {
        connect(check, &QCheckBox::toggled, this,
                &PrayerSettingsDialog::markDirty);
    }
    for (QTimeEdit *time : findChildren<QTimeEdit *>()) {
        connect(time, &QTimeEdit::timeChanged, this,
                &PrayerSettingsDialog::markDirty);
    }
    // Selecting a country only fills the city list; choosing a city changes
    // the coordinates and is a real edit.
    if (m_cities) {
        connect(m_cities, &QListWidget::currentRowChanged, this,
                &PrayerSettingsDialog::markDirty);
        connect(m_cities, &QListWidget::itemClicked, this,
                [this](QListWidgetItem *) { markDirty(); });
    }
}

void PrayerSettingsDialog::setTabIndex(int index) {
    if (m_nav) {
        m_nav->setCurrentRow(qBound(0, index, m_nav->count() - 1));
    }
}

void PrayerSettingsDialog::reload() {
    populate(configObject(m_core));
}

void PrayerSettingsDialog::populate(const QJsonObject &config) {
    m_updating = true;
    m_populating = true;
    m_city->setText(config.value(QStringLiteral("city")).toString());
    m_province->setText(config.value(QStringLiteral("province")).toString());
    m_latitude->setValue(config.value(QStringLiteral("latitude")).toDouble());
    m_longitude->setValue(config.value(QStringLiteral("longitude")).toDouble());
    m_timezone->setValue(config.value(QStringLiteral("timezone")).toDouble());

    const QString zoneId = config.value(QStringLiteral("timezone_name")).toString();
    int zoneIndex = m_timeZoneName->findData(zoneId);
    m_timeZoneName->setCurrentIndex(qMax(0, zoneIndex));

    int dstIndex = m_dstMode->findData(
        config.value(QStringLiteral("dst_mode")).toInt());
    m_dstMode->setCurrentIndex(qMax(0, dstIndex));

    int methodIndex = m_method->findData(config.value(QStringLiteral("method")).toInt());
    m_method->setCurrentIndex(qMax(0, methodIndex));
    int mathhabIndex = m_mathhab->findData(config.value(QStringLiteral("mathhab")).toInt());
    m_mathhab->setCurrentIndex(qMax(0, mathhabIndex));
    int roundingIndex = m_rounding->findData(config.value(QStringLiteral("round")).toInt());
    m_rounding->setCurrentIndex(qMax(0, roundingIndex));
    int extremeIndex = m_extreme->findData(config.value(QStringLiteral("extreme")).toInt());
    m_extreme->setCurrentIndex(qMax(0, extremeIndex));
    m_fajrAngle->setValue(config.value(QStringLiteral("fajr_angle")).toDouble());
    m_ishaaAngle->setValue(config.value(QStringLiteral("ishaa_angle")).toDouble());
    m_dhuhrMinutes->setValue(config.value(QStringLiteral("dhuhr_minutes")).toInt());
    m_maghribMinutes->setValue(config.value(QStringLiteral("maghrib_minutes")).toInt());

    const QJsonArray adjust = config.value(QStringLiteral("adjust")).toArray();
    for (int index = 0; index < 6; ++index) {
        m_adjust[index]->setValue(index < adjust.size() ? adjust.at(index).toInt() : 0);
    }

    updateMethodDescription();
    updateExtremeDescription();
    loadCountries();
    m_populating = false;
    m_updating = false;
    if (m_applyButton) {
        m_applyButton->setEnabled(false);
    }
}

void PrayerSettingsDialog::loadCountries() {
    const QString text = takeCoreString(namaz_core_countries_json());
    const QJsonArray countries = QJsonDocument::fromJson(text.toUtf8()).array();
    m_updating = true;
    m_countries->clear();
    for (const auto &country : countries) {
        m_countries->addItem(country.toString());
    }

    const QString current = m_city->text();
    int selected = 0;
    // Match ", Country" instead of a bare suffix: "Guinea" is also a suffix of
    // "Equatorial Guinea" and "Papua New Guinea", and "Jan Mayen" of
    // "Svalbard And Jan Mayen".
    for (int index = 0; index < m_countries->count(); ++index) {
        const QString country = m_countries->item(index)->text();
        if (current.endsWith(QStringLiteral(", ") + country, Qt::CaseInsensitive)) {
            selected = index;
            break;
        }
    }
    m_countries->setCurrentRow(selected);
    m_updating = false;
    loadCities(m_countries->currentItem() ? m_countries->currentItem()->text() : QString(),
               false);
}

void PrayerSettingsDialog::loadCities(const QString &country, bool apply) {
    m_updating = true;
    m_cities->clear();
    const QByteArray countryUtf8 = country.toUtf8();
    const QString text = takeCoreString(
        namaz_core_country_cities_json(countryUtf8.constData()));
    const QJsonArray cities = QJsonDocument::fromJson(text.toUtf8()).array();
    for (const auto &value : cities) {
        const QJsonObject city = value.toObject();
        const QString name = city.value(QStringLiteral("name")).toString();
        const QString state = city.value(QStringLiteral("state")).toString();
        QString display = name;
        if (!state.isEmpty() && name != state && !name.contains(QStringLiteral(" | "))) {
            display = QStringLiteral("%1 | %2").arg(name, state);
        }
        auto *item = new QListWidgetItem(display);
        item->setData(CityNameRole, name);
        item->setData(CityStateRole, state);
        item->setData(CityLatRole, city.value(QStringLiteral("lat")).toDouble());
        item->setData(CityLonRole, city.value(QStringLiteral("lon")).toDouble());
        item->setData(CityTzRole, city.value(QStringLiteral("tz")).toDouble());
        item->setData(CityTzNameRole, city.value(QStringLiteral("tz_name")).toString());
        m_cities->addItem(item);
    }

    // Prefer the coordinates: they identify the city unambiguously even when
    // the database contains several cities with the same name.
    const double latitude = m_latitude->value();
    const double longitude = m_longitude->value();
    int selected = -1;
    for (int index = 0; index < m_cities->count(); ++index) {
        const QListWidgetItem *item = m_cities->item(index);
        if (qAbs(item->data(CityLatRole).toDouble() - latitude) < 0.002
            && qAbs(item->data(CityLonRole).toDouble() - longitude) < 0.002) {
            selected = index;
            break;
        }
    }
    m_cities->setCurrentRow(selected);
    m_updating = false;
    if (apply && !m_populating) {
        selectCity();
    }
}

void PrayerSettingsDialog::selectCity() {
    auto *countryItem = m_countries->currentItem();
    auto *cityItem = m_cities->currentItem();
    if (!countryItem || !cityItem) {
        return;
    }
    const QString name = cityItem->data(CityNameRole).toString();
    const QString state = cityItem->data(CityStateRole).toString();
    const QString zoneName = cityItem->data(CityTzNameRole).toString();

    m_updating = true;
    m_latitude->setValue(cityItem->data(CityLatRole).toDouble());
    m_longitude->setValue(cityItem->data(CityLonRole).toDouble());
    m_timezone->setValue(cityItem->data(CityTzRole).toDouble());
    const int zoneIndex = m_timeZoneName->findData(zoneName);
    m_timeZoneName->setCurrentIndex(zoneIndex >= 0 ? zoneIndex : 0);

    int dstMode = 0;
    if (zoneIndex > 0) {
        const QTimeZone zone(zoneName.toUtf8());
        if (zone.isValid() && zone.hasDaylightTime()) {
            dstMode = 1;
        }
    }
    const int dstIndex = m_dstMode->findData(dstMode);
    if (dstIndex >= 0) {
        m_dstMode->setCurrentIndex(dstIndex);
    }
    m_updating = false;

    QString cityText = name;
    if (!state.isEmpty() && !name.contains(QStringLiteral(" | "))) {
        cityText += QStringLiteral(" | ") + state;
    }
    m_city->setText(cityText + QStringLiteral(", ") + countryItem->text());
    m_province->setText(state);
}

void PrayerSettingsDialog::updateMethodDescription() {
    static const char *descriptions[] = {
        QT_TR_NOOP("Custom twilight angles: Fajr and Ishaa use the values below."),
        QT_TR_NOOP("Egyptian General Authority of Survey: Fajr 19.5°, Ishaa 17.5°."),
        QT_TR_NOOP("Islamic Society of North America: Fajr 15°, Ishaa 15°."),
        QT_TR_NOOP("Muslim World League: Fajr 18°, Ishaa 17°."),
        QT_TR_NOOP("Umm al-Qura: Fajr 15°, Ishaa 90 minutes after Maghrib."),
        QT_TR_NOOP("University of Islamic Sciences, Karachi: Fajr 18°, Ishaa 18°."),
    };
    const int index = qBound(0, m_method->currentIndex(), 5);
    m_methodDescription->setText(tr(descriptions[index]));
    // Preset methods define their own twilight angles; the spin boxes were
    // editable but silently ignored.  Only the custom method uses them.
    const bool custom = m_method->currentData().toInt() == 0;
    const QString angleHint = tr("Only used with Custom twilight angles.");
    m_fajrAngle->setEnabled(custom);
    m_ishaaAngle->setEnabled(custom);
    m_fajrAngle->setToolTip(angleHint);
    m_ishaaAngle->setToolTip(angleHint);
}

void PrayerSettingsDialog::updateExtremeDescription() {
    static const char *descriptions[] = {
        QT_TR_NOOP("No extreme-latitude adjustment."),
        QT_TR_NOOP("Place Fajr and Ishaa in the middle of the night."),
        QT_TR_NOOP("Use the middle of the night only when the angle is invalid."),
        QT_TR_NOOP("Place Fajr and Ishaa one-seventh of the night from the horizon."),
        QT_TR_NOOP("Use one-seventh of the night only when the angle is invalid."),
        QT_TR_NOOP("Use the nearest day (Aqrab Al-Ayyam) with valid twilight times."),
    };
    const int index = qBound(0, m_extreme->currentIndex(), 5);
    m_extremeDescription->setText(tr(descriptions[index]));
}

bool PrayerSettingsDialog::validate() {
    if (m_dstMode->currentData().toInt() == 1) {
        const QString id = m_timeZoneName->currentData().toString();
        if (id.isEmpty()) {
            QMessageBox::warning(
                this, tr("Daylight Saving"),
                tr("Automatic daylight saving requires an IANA time zone. "
                   "Select a time zone or choose another DST mode."));
            return false;
        }
        const QTimeZone zone(id.toUtf8());
        if (zone.isValid()) {
            // In automatic mode the numeric field stores standard time, not
            // the current summer offset.
            const double offset =
                zone.standardTimeOffset(QDateTime::currentDateTime()) / 3600.0;
            if (qAbs(offset - m_timezone->value()) > 0.01) {
                const auto answer = QMessageBox::question(
                    this, tr("Time Zone"),
                    tr("The numeric UTC offset (%1) does not match the selected "
                       "time zone (%2).\n\nSave anyway?")
                        .arg(m_timezone->value())
                        .arg(offset));
                if (answer != QMessageBox::Yes) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool PrayerSettingsDialog::apply() {
    QJsonObject config;
    config.insert(QStringLiteral("city"), m_city->text());
    config.insert(QStringLiteral("province"), m_province->text());
    config.insert(QStringLiteral("latitude"), m_latitude->value());
    config.insert(QStringLiteral("longitude"), m_longitude->value());
    config.insert(QStringLiteral("timezone"), m_timezone->value());
    config.insert(QStringLiteral("timezone_name"),
                  m_timeZoneName->currentData().toString());
    config.insert(QStringLiteral("dst_mode"), m_dstMode->currentData().toInt());
    // The UI only offers "No DST", "Automatic" and "Manual +1 hour"; the old
    // separate 0/1-hour spinner could contradict the selected mode.
    config.insert(QStringLiteral("dst_manual"),
                  m_dstMode->currentData().toInt() == 2 ? 1 : 0);
    config.insert(QStringLiteral("method"), m_method->currentData().toInt());
    config.insert(QStringLiteral("mathhab"), m_mathhab->currentData().toInt());
    config.insert(QStringLiteral("round"), m_rounding->currentData().toInt());
    config.insert(QStringLiteral("extreme"), m_extreme->currentData().toInt());
    config.insert(QStringLiteral("fajr_angle"), m_fajrAngle->value());
    config.insert(QStringLiteral("ishaa_angle"), m_ishaaAngle->value());
    config.insert(QStringLiteral("dhuhr_minutes"), m_dhuhrMinutes->value());
    config.insert(QStringLiteral("maghrib_minutes"), m_maghribMinutes->value());
    QJsonArray adjust;
    for (int index = 0; index < 6; ++index) {
        adjust.append(m_adjust[index]->value());
    }
    config.insert(QStringLiteral("adjust"), adjust);

    const QByteArray json = QJsonDocument(config).toJson(QJsonDocument::Compact);
    if (namaz_core_apply_settings_json(m_core, json.constData())) {
        if (m_applyButton) {
            m_applyButton->setEnabled(false);
        }
        emit applied();
        return true;
    }
    QMessageBox::warning(
        this, tr("Prayer Settings"),
        tr("Could not save the configuration file. Check the permissions of "
           "the configuration directory."));
    return false;
}

PreferencesDialog::PreferencesDialog(void *core, QWidget *parent)
    : QDialog(parent), m_core(core) {
    setWindowTitle(tr("Namaz — Preferences"));
    setModal(true);
    setMinimumSize(720, 460);
    resize(900, 740);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(Ui::groupSpacing());

    Navigation nav = createNavigation(this, layout, QStringLiteral("Preferences"));
    m_nav = nav.list;
    m_stack = nav.stack;

    const QStringList sounds = availableAthanSounds();
    const QString prayerNames[] = {tr("Fajr"), tr("Shuruq"), tr("Dhuhr"),
                                   tr("Asr"), tr("Maghrib"), tr("Ishaa")};

    auto makePathRow = [this](QWidget *parentWidget, QLineEdit *&edit,
                              const QString &placeholder, bool folder) -> QWidget * {
        auto *row = new QWidget(parentWidget);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(Ui::controlSpacing());
        edit = new QLineEdit(row);
        edit->setClearButtonEnabled(true);
        edit->setPlaceholderText(placeholder);
        auto *browse = smallButton(row,
                                   folder ? QStringLiteral("document-open-folder")
                                          : QStringLiteral("document-open"),
                                   folder ? tr("Browse for a folder") : tr("Browse for a WAV file"));
        auto *play = smallButton(row, QStringLiteral("media-playback-start"),
                                 folder ? tr("Play a random file") : tr("Play preview"));
        auto *stop = smallButton(row, QStringLiteral("media-playback-stop"),
                                 tr("Stop preview"));
        rowLayout->addWidget(edit, 1);
        rowLayout->addWidget(browse);
        rowLayout->addWidget(play);
        rowLayout->addWidget(stop);
        connect(browse, &QToolButton::clicked, this, [this, edit, folder] {
            if (folder) {
                browseFolder(edit);
            } else {
                browseFile(edit);
            }
        });
        connect(play, &QToolButton::clicked, this, [this, edit, folder] {
            if (folder) {
                previewFolder(edit);
            } else {
                previewFile(edit);
            }
        });
        connect(stop, &QToolButton::clicked, [] { namaz_core_stop_audio(); });
        return row;
    };

    // ── Audio ───────────────────────────────────────────────────────────
    Page audioPage = addScrollPage(nav, tr("Audio Settings"),
                                   QStringLiteral("audio-volume-high"));
    auto *audioLayout = audioPage.layout;

    auto *generalBox = compactBox(tr("General"), audioPage.widget);
    auto *generalLayout = new QVBoxLayout(generalBox);
    generalLayout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                                      Ui::pageMargin(), Ui::pageMargin());
    generalLayout->setSpacing(Ui::controlSpacing());
    m_athanEnabled = new QCheckBox(tr("Enable audio (athan, reminders and startup)"), generalBox);
    m_athanEnabled->setObjectName(QStringLiteral("AthanEnabledCheck"));
    m_notifications = new QCheckBox(tr("Show desktop notifications"), generalBox);
    m_notifications->setObjectName(QStringLiteral("NotificationsCheck"));
    m_stopOnPrayer = new QCheckBox(tr("Stop other audio when a prayer starts"), generalBox);
    generalLayout->addWidget(m_athanEnabled);
    generalLayout->addWidget(m_notifications);
    generalLayout->addWidget(m_stopOnPrayer);
    generalLayout->addWidget(volumeRow(generalBox, m_volume, m_volumeValue));
    connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
        // Do not apply the volume while editing: Cancel must leave the
        // current session untouched.  The value is applied by Apply/OK.
        m_volumeValue->setText(QStringLiteral("%1%").arg(value));
    });
    audioLayout->addWidget(generalBox);

    auto *prayerBox = compactBox(tr("Athan per prayer"), audioPage.widget);
    auto *prayerGrid = new QGridLayout(prayerBox);
    Ui::compactGrid(prayerGrid);
    for (int index = 0; index < 6; ++index) {
        auto *name = new QLabel(prayerNames[index], prayerBox);
        name->setMinimumWidth(64);

        m_audioMode[index] = new QComboBox(prayerBox);
        m_audioMode[index]->setObjectName(QStringLiteral("AthanMode%1").arg(index));
        m_audioMode[index]->addItem(tr("No Athan"), 0);
        m_audioMode[index]->addItem(tr("Selected Athan"), 1);
        m_audioMode[index]->addItem(tr("Custom Athan"), 2);
        m_audioMode[index]->addItem(tr("Custom Folder"), 3);
        m_audioMode[index]->setAccessibleName(
            tr("Athan mode for %1").arg(prayerNames[index]));
        Ui::setFieldWidth(m_audioMode[index], 170);

        m_audioStack[index] = new QStackedWidget(prayerBox);

        // A plain label looked like accidental text between the combo boxes.
        // Give it the same field-like surface as the controls around it.
        auto *empty = new QLabel(tr("No athan will be played."), prayerBox);
        empty->setContentsMargins(Ui::controlSpacing(), 4,
                                  Ui::controlSpacing(), 4);
        empty->setAutoFillBackground(true);
        empty->setFrameShape(QFrame::StyledPanel);
        empty->setFrameShadow(QFrame::Plain);
        QPalette emptyPalette = empty->palette();
        emptyPalette.setColor(QPalette::Window,
                              emptyPalette.color(QPalette::Base));
        empty->setPalette(emptyPalette);

        auto *selectedPage = new QWidget(prayerBox);
        auto *selectedLayout = new QHBoxLayout(selectedPage);
        selectedLayout->setContentsMargins(0, 0, 0, 0);
        selectedLayout->setSpacing(Ui::controlSpacing());
        m_sounds[index] = soundCombo(selectedPage, sounds, QString());
        auto *selectedPlay = smallButton(selectedPage,
                                         QStringLiteral("media-playback-start"),
                                         tr("Play preview"));
        auto *selectedStop = smallButton(selectedPage,
                                         QStringLiteral("media-playback-stop"),
                                         tr("Stop preview"));
        selectedLayout->addWidget(m_sounds[index], 1);
        selectedLayout->addWidget(selectedPlay);
        selectedLayout->addWidget(selectedStop);
        connect(selectedPlay, &QToolButton::clicked, this, [this, index] {
            previewSound(m_sounds[index]->currentData().toString());
        });
        connect(selectedStop, &QToolButton::clicked, [] { namaz_core_stop_audio(); });

        QWidget *filePage = makePathRow(prayerBox, m_soundFile[index],
                                        tr("Path to a WAV file"), false);
        QWidget *folderPage = makePathRow(prayerBox, m_soundFolder[index],
                                          tr("Folder with WAV files"), true);

        m_audioStack[index]->addWidget(empty);
        m_audioStack[index]->addWidget(selectedPage);
        m_audioStack[index]->addWidget(filePage);
        m_audioStack[index]->addWidget(folderPage);

        prayerGrid->addWidget(name, index, 0);
        prayerGrid->addWidget(m_audioMode[index], index, 1);
        prayerGrid->addWidget(m_audioStack[index], index, 2);
        connect(m_audioMode[index], &QComboBox::currentIndexChanged, this,
                [this, index](int) { updateAudioRow(index); });
    }
    // Sunrise is not a prayer with an athan.
    m_audioMode[1]->setEnabled(false);
    m_audioStack[1]->setEnabled(false);
    m_audioMode[1]->setToolTip(tr("Shuruq is sunrise; no athan is played."));

    prayerGrid->setColumnStretch(2, 1);
    audioLayout->addWidget(prayerBox);

    auto *startupBox = compactBox(tr("On startup"), audioPage.widget);
    auto *startupForm = compactForm(startupBox);
    m_startupMode = new QComboBox(startupBox);
    m_startupMode->setObjectName(QStringLiteral("StartupModeCombo"));
    m_startupMode->addItem(tr("No audio"), 0);
    m_startupMode->addItem(tr("Bismi Allah"), 1);
    m_startupMode->addItem(tr("Quran"), 2);
    m_startupMode->addItem(tr("Custom file"), 3);
    m_startupMode->addItem(tr("Custom folder"), 4);
    Ui::setFieldWidth(m_startupMode, 220);
    startupForm->addRow(tr("Sound"), m_startupMode);

    m_startupStack = new QStackedWidget(startupBox);
    for (int mode = 0; mode < 3; ++mode) {
        auto *empty = new QLabel(mode == 0 ? tr("No startup sound.")
                                           : tr("Uses the built-in sound."),
                                 startupBox);
        empty->setForegroundRole(QPalette::PlaceholderText);
        m_startupStack->addWidget(empty);
    }
    m_startupStack->addWidget(makePathRow(startupBox, m_startupFile,
                                          tr("Path to a WAV file"), false));
    m_startupStack->addWidget(makePathRow(startupBox, m_startupFolder,
                                          tr("Folder with WAV files"), true));
    startupForm->addRow(tr("Custom sound"), m_startupStack);
    connect(m_startupMode, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateStartupRow);
    audioLayout->addWidget(startupBox);
    audioLayout->addStretch(1);

    // ── Reminders ───────────────────────────────────────────────────────
    Page remindersPage = addScrollPage(nav, tr("Reminders"),
                                       QStringLiteral("appointment-reminder"));
    auto *remindersLayout = remindersPage.layout;

    auto *makeSameRow = new QWidget(remindersPage.widget);
    auto *makeSameLayout = new QHBoxLayout(makeSameRow);
    makeSameLayout->setContentsMargins(0, 0, 0, 0);
    makeSameLayout->setSpacing(Ui::controlSpacing());
    auto *makeAll = new QPushButton(tr("Make all the same"), makeSameRow);
    auto *makeLate = new QPushButton(tr("Make Asr, Maghrib and Ishaa the same"),
                                     makeSameRow);
    makeSameLayout->addWidget(makeAll);
    makeSameLayout->addWidget(makeLate);
    makeSameLayout->addStretch(1);
    remindersLayout->addWidget(makeSameRow);

    auto *reminderBox = compactBox(tr("Reminders"), remindersPage.widget);
    auto *reminderGrid = new QGridLayout(reminderBox);
    Ui::compactGrid(reminderGrid);
    const QString headers[] = {tr("Prayer"), tr("Before"), tr("After"),
                               tr("Fixed time"), tr("Iqama")};
    for (int column = 0; column < 5; ++column) {
        auto *header = new QLabel(headers[column], reminderBox);
        Ui::styleCaption(header);
        reminderGrid->addWidget(header, 0, column);
    }
    for (int index = 0; index < 6; ++index) {
        reminderGrid->addWidget(new QLabel(prayerNames[index], reminderBox), index + 1, 0);

        auto *beforeRow = new QWidget(reminderBox);
        auto *beforeLayout = new QHBoxLayout(beforeRow);
        beforeLayout->setContentsMargins(0, 0, 0, 0);
        beforeLayout->setSpacing(Ui::controlSpacing());
        m_beforeOn[index] = new QCheckBox(beforeRow);
        m_beforeMinutes[index] = minutesSpin(beforeRow);
        Ui::setFieldWidth(m_beforeMinutes[index], 86);
        beforeLayout->addWidget(m_beforeOn[index]);
        beforeLayout->addWidget(m_beforeMinutes[index]);
        reminderGrid->addWidget(beforeRow, index + 1, 1);

        auto *afterRow = new QWidget(reminderBox);
        auto *afterLayout = new QHBoxLayout(afterRow);
        afterLayout->setContentsMargins(0, 0, 0, 0);
        afterLayout->setSpacing(Ui::controlSpacing());
        m_afterOn[index] = new QCheckBox(afterRow);
        m_afterMinutes[index] = minutesSpin(afterRow);
        Ui::setFieldWidth(m_afterMinutes[index], 86);
        afterLayout->addWidget(m_afterOn[index]);
        afterLayout->addWidget(m_afterMinutes[index]);
        reminderGrid->addWidget(afterRow, index + 1, 2);

        auto *fixedRow = new QWidget(reminderBox);
        auto *fixedLayout = new QHBoxLayout(fixedRow);
        fixedLayout->setContentsMargins(0, 0, 0, 0);
        fixedLayout->setSpacing(Ui::controlSpacing());
        m_fixedOn[index] = new QCheckBox(fixedRow);
        m_fixedTime[index] = new QTimeEdit(fixedRow);
        m_fixedTime[index]->setDisplayFormat(QStringLiteral("HH:mm"));
        Ui::setFieldWidth(m_fixedTime[index], 88);
        fixedLayout->addWidget(m_fixedOn[index]);
        fixedLayout->addWidget(m_fixedTime[index]);
        reminderGrid->addWidget(fixedRow, index + 1, 3);

        auto *iqamaRow = new QWidget(reminderBox);
        auto *iqamaLayout = new QHBoxLayout(iqamaRow);
        iqamaLayout->setContentsMargins(0, 0, 0, 0);
        iqamaLayout->setSpacing(Ui::controlSpacing());
        m_iqamaOn[index] = new QCheckBox(iqamaRow);
        m_iqamaOn[index]->setObjectName(QStringLiteral("IqamaOn%1").arg(index));
        m_iqamaMinutes[index] = minutesSpin(iqamaRow);
        m_iqamaMinutes[index]->setObjectName(QStringLiteral("IqamaMinutes%1").arg(index));
        Ui::setFieldWidth(m_iqamaMinutes[index], 86);
        iqamaLayout->addWidget(m_iqamaOn[index]);
        iqamaLayout->addWidget(m_iqamaMinutes[index]);
        reminderGrid->addWidget(iqamaRow, index + 1, 4);

        m_beforeOn[index]->setAccessibleName(
            tr("Enable reminder before %1").arg(prayerNames[index]));
        m_beforeMinutes[index]->setAccessibleName(
            tr("Minutes before %1").arg(prayerNames[index]));
        m_afterOn[index]->setAccessibleName(
            tr("Enable reminder after %1").arg(prayerNames[index]));
        m_afterMinutes[index]->setAccessibleName(
            tr("Minutes after %1").arg(prayerNames[index]));
        m_fixedOn[index]->setAccessibleName(
            tr("Enable fixed reminder for %1").arg(prayerNames[index]));
        m_fixedTime[index]->setAccessibleName(
            tr("Fixed time for %1").arg(prayerNames[index]));
        m_iqamaOn[index]->setAccessibleName(
            tr("Enable iqama reminder for %1").arg(prayerNames[index]));
        m_iqamaMinutes[index]->setAccessibleName(
            tr("Iqama minutes for %1").arg(prayerNames[index]));
    }
    // Sunrise has no iqama.
    m_iqamaOn[1]->setEnabled(false);
    m_iqamaMinutes[1]->setEnabled(false);
    m_iqamaOn[1]->setChecked(false);
    m_iqamaMinutes[1]->setValue(0);
    remindersLayout->addWidget(reminderBox);

    connect(makeAll, &QPushButton::clicked, this, [this] {
        for (int index = 1; index < 6; ++index) {
            m_beforeOn[index]->setChecked(m_beforeOn[0]->isChecked());
            m_beforeMinutes[index]->setValue(m_beforeMinutes[0]->value());
            m_afterOn[index]->setChecked(m_afterOn[0]->isChecked());
            m_afterMinutes[index]->setValue(m_afterMinutes[0]->value());
            m_fixedOn[index]->setChecked(m_fixedOn[0]->isChecked());
            m_fixedTime[index]->setTime(m_fixedTime[0]->time());
            if (index != 1) {
                m_iqamaOn[index]->setChecked(m_iqamaOn[0]->isChecked());
                m_iqamaMinutes[index]->setValue(m_iqamaMinutes[0]->value());
            }
        }
    });
    connect(makeLate, &QPushButton::clicked, this, [this] {
        for (int index = 4; index < 6; ++index) {
            m_beforeOn[index]->setChecked(m_beforeOn[3]->isChecked());
            m_beforeMinutes[index]->setValue(m_beforeMinutes[3]->value());
            m_afterOn[index]->setChecked(m_afterOn[3]->isChecked());
            m_afterMinutes[index]->setValue(m_afterMinutes[3]->value());
            m_fixedOn[index]->setChecked(m_fixedOn[3]->isChecked());
            m_fixedTime[index]->setTime(m_fixedTime[3]->time());
            m_iqamaOn[index]->setChecked(m_iqamaOn[3]->isChecked());
            m_iqamaMinutes[index]->setValue(m_iqamaMinutes[3]->value());
        }
    });

    auto *visualBox = compactBox(tr("Reminder Visual Alerts"), remindersPage.widget);
    auto *visualLayout = new QVBoxLayout(visualBox);
    visualLayout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                                     Ui::pageMargin(), Ui::pageMargin());
    visualLayout->setSpacing(Ui::controlSpacing());
    m_reminderNotification = new QCheckBox(tr("Display desktop notification"), visualBox);
    m_reminderMain = new QCheckBox(tr("Display the main window"), visualBox);
    m_reminderFlash = new QCheckBox(tr("Flash the tray icon"), visualBox);
    visualLayout->addWidget(m_reminderNotification);
    visualLayout->addWidget(m_reminderMain);
    visualLayout->addWidget(m_reminderFlash);
    remindersLayout->addWidget(visualBox);

    auto *reminderAudioBox = compactBox(tr("Reminder Audio Alerts"),
                                        remindersPage.widget);
    auto *reminderAudioForm = compactForm(reminderAudioBox);
    m_reminderAudioMode = new QComboBox(reminderAudioBox);
    m_reminderAudioMode->setObjectName(QStringLiteral("ReminderAudioModeCombo"));
    m_reminderAudioMode->addItem(tr("No audio"), 0);
    m_reminderAudioMode->addItem(tr("Bismi Allah"), 2);
    m_reminderAudioMode->addItem(tr("Quran"), 3);
    m_reminderAudioMode->addItem(tr("Custom reminder file"), 4);
    m_reminderAudioMode->addItem(tr("Custom reminder folder"), 5);
    Ui::setFieldWidth(m_reminderAudioMode, 220);
    reminderAudioForm->addRow(tr("Sound"), m_reminderAudioMode);
    m_reminderFileRow = makePathRow(reminderAudioBox, m_reminderFile,
                                    tr("Path to a WAV file"), false);
    m_reminderFolderRow = makePathRow(reminderAudioBox, m_reminderFolder,
                                      tr("Folder with WAV files"), true);
    reminderAudioForm->addRow(tr("Custom file"), m_reminderFileRow);
    reminderAudioForm->addRow(tr("Custom folder"), m_reminderFolderRow);
    m_reminderFileLabel = reminderAudioForm->labelForField(m_reminderFileRow);
    m_reminderFolderLabel = reminderAudioForm->labelForField(m_reminderFolderRow);
    auto *reminderInfo = descriptionLabel(reminderAudioBox);
    reminderInfo->setText(tr("Reminder audio is played for before/after, fixed-time "
                             "and iqama reminders."));
    reminderAudioForm->addRow(QString(), reminderInfo);
    connect(m_reminderAudioMode, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateReminderAudioRow);
    remindersLayout->addWidget(reminderAudioBox);
    remindersLayout->addStretch(1);

    // ── Other ───────────────────────────────────────────────────────────
    Page otherPage = addScrollPage(nav, tr("Other Settings"),
                                   QStringLiteral("preferences-other"));
    auto *otherLayout = otherPage.layout;
    auto *otherGrid = new QGridLayout();
    Ui::compactGrid(otherGrid);

    auto *windowBox = compactBox(tr("Window"), otherPage.widget);
    auto *windowLayout = new QVBoxLayout(windowBox);
    windowLayout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                                     Ui::pageMargin(), Ui::pageMargin());
    windowLayout->setSpacing(Ui::controlSpacing());
    m_alwaysOnTop = new QCheckBox(tr("Always on top"), windowBox);
    m_hideOnClose = new QCheckBox(tr("Keep running in the system tray when closed"), windowBox);
    m_launchMinimized = new QCheckBox(tr("Start minimized"), windowBox);
    m_launchOnStartup = new QCheckBox(tr("Start automatically on login"), windowBox);
    windowLayout->addWidget(m_alwaysOnTop);
    windowLayout->addWidget(m_hideOnClose);
    windowLayout->addWidget(m_launchMinimized);
    windowLayout->addWidget(m_launchOnStartup);
    otherGrid->addWidget(windowBox, 0, 0);

    auto *alertsBox = compactBox(tr("Prayer Visual Alerts"), otherPage.widget);
    auto *alertsLayout = new QVBoxLayout(alertsBox);
    alertsLayout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                                     Ui::pageMargin(), Ui::pageMargin());
    alertsLayout->setSpacing(Ui::controlSpacing());
    m_athanVisualMain = new QCheckBox(tr("Display the main window"), alertsBox);
    m_athanVisualFlash = new QCheckBox(tr("Flash the tray icon"), alertsBox);
    alertsLayout->addWidget(m_athanVisualMain);
    alertsLayout->addWidget(m_athanVisualFlash);
    alertsLayout->addStretch(1);
    otherGrid->addWidget(alertsBox, 0, 1);

    auto *calendarBox = compactBox(tr("Calendar"), otherPage.widget);
    auto *calendarLayout = new QVBoxLayout(calendarBox);
    calendarLayout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                                       Ui::pageMargin(), Ui::pageMargin());
    calendarLayout->setSpacing(Ui::controlSpacing());
    m_hijriCalendar = new QCheckBox(tr("Show the Hijri date on the main screen"),
                                    calendarBox);
    auto *hijriRow = new QWidget(calendarBox);
    auto *hijriLayout = new QHBoxLayout(hijriRow);
    hijriLayout->setContentsMargins(0, 0, 0, 0);
    hijriLayout->setSpacing(Ui::controlSpacing());
    hijriLayout->addWidget(new QLabel(tr("Hijri date offset"), hijriRow));
    m_hijriOffset = new QSpinBox(hijriRow);
    m_hijriOffset->setRange(-3, 3);
    m_hijriOffset->setSuffix(tr(" day(s)"));
    Ui::setFieldWidth(m_hijriOffset, 140);
    hijriLayout->addWidget(m_hijriOffset);
    hijriLayout->addStretch(1);
    calendarLayout->addWidget(m_hijriCalendar);
    calendarLayout->addWidget(hijriRow);

    auto *timeFormatRow = new QWidget(calendarBox);
    auto *timeFormatLayout = new QHBoxLayout(timeFormatRow);
    timeFormatLayout->setContentsMargins(0, 0, 0, 0);
    timeFormatLayout->setSpacing(Ui::controlSpacing());
    timeFormatLayout->addWidget(new QLabel(tr("Time format"), timeFormatRow));
    m_timeFormat = new QComboBox(timeFormatRow);
    m_timeFormat->setObjectName(QStringLiteral("TimeFormatCombo"));
    m_timeFormat->addItem(tr("12-hour: hh:mm AM/PM"), 1);
    m_timeFormat->addItem(tr("12-hour: hh:mm:ss AM/PM"), 0);
    m_timeFormat->addItem(tr("24-hour: HH:mm"), 2);
    m_timeFormat->addItem(tr("24-hour: HH:mm:ss"), 3);
    m_timeFormat->setToolTip(
        tr("12-hour or 24-hour clock; the current time can also show seconds."));
    Ui::setFieldWidth(m_timeFormat, 230);
    timeFormatLayout->addWidget(m_timeFormat);
    timeFormatLayout->addStretch(1);
    calendarLayout->addWidget(timeFormatRow);
    otherGrid->addWidget(calendarBox, 1, 0);

    auto *qiblahBox = compactBox(tr("Qiblah"), otherPage.widget);
    auto *qiblahLayout = new QVBoxLayout(qiblahBox);
    qiblahLayout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                                     Ui::pageMargin(), Ui::pageMargin());
    qiblahLayout->setSpacing(Ui::controlSpacing());
    m_showQiblah = new QCheckBox(tr("Show the Qiblah panel"), qiblahBox);
    m_qiblaClockwise = new QCheckBox(tr("Show the angle clockwise from North"), qiblahBox);
    qiblahLayout->addWidget(m_showQiblah);
    qiblahLayout->addWidget(m_qiblaClockwise);
    qiblahLayout->addStretch(1);
    otherGrid->addWidget(qiblahBox, 1, 1);

    auto *masjidBox = compactBox(tr("Masjid Screen"), otherPage.widget);
    auto *masjidForm = compactForm(masjidBox);
    m_masjidScreen = new QComboBox(masjidBox);
    Ui::setFieldWidth(m_masjidScreen, 260);
    masjidForm->addRow(tr("Display on"), m_masjidScreen);
    auto *masjidNote = descriptionLabel(masjidBox);
    masjidNote->setText(
        tr("Shows the clock, the next prayer and today's times full-screen."));
    masjidForm->addRow(QString(), masjidNote);
    otherGrid->addWidget(masjidBox, 2, 0);

    auto *securityBox = compactBox(tr("Security"), otherPage.widget);
    auto *securityLayout = new QVBoxLayout(securityBox);
    securityLayout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                                       Ui::pageMargin(), Ui::pageMargin());
    securityLayout->setSpacing(Ui::controlSpacing());
    m_lockOnPray = new QCheckBox(tr("Lock screen on Pray Now"), securityBox);
    m_screensaverOnPray = new QCheckBox(tr("Screen saver on Pray Now"),
                                        securityBox);
    m_showSplash = new QCheckBox(tr("Show splash on startup"), securityBox);
    securityLayout->addWidget(m_lockOnPray);
    securityLayout->addWidget(m_screensaverOnPray);
    securityLayout->addWidget(m_showSplash);
    otherGrid->addWidget(securityBox, 2, 1);

    auto *integrationBox = compactBox(tr("Integration"), otherPage.widget);
    auto *integrationForm = compactForm(integrationBox);
    m_dbusEnabled = new QCheckBox(tr("Enable the D-Bus interface"), integrationBox);
    m_dbusEnabled->setObjectName(QStringLiteral("DbusEnabledCheck"));
    m_dbusInterval = new QSpinBox(integrationBox);
    m_dbusInterval->setRange(5, 3600);
    m_dbusInterval->setSuffix(tr(" s"));
    Ui::setFieldWidth(m_dbusInterval, 120);
    integrationForm->addRow(m_dbusEnabled);
    integrationForm->addRow(tr("D-Bus update interval"), m_dbusInterval);
    otherGrid->addWidget(integrationBox, 3, 0, 1, 2);

    auto *languageBox = compactBox(tr("Language"), otherPage.widget);
    auto *languageForm = compactForm(languageBox);
    m_language = new QComboBox(languageBox);
    m_language->setObjectName(QStringLiteral("LanguageCombo"));
    m_language->addItem(tr("System language"), QStringLiteral("system"));
    m_language->addItem(tr("English"), QStringLiteral("en"));
    m_language->addItem(tr("Русский"), QStringLiteral("ru"));
    m_language->setToolTip(tr("Changing the language restarts Namaz."));
    Ui::setFieldWidth(m_language, 220);
    languageForm->addRow(tr("Interface language"), m_language);
    otherGrid->addWidget(languageBox, 4, 0, 1, 2);

    otherGrid->setColumnStretch(0, 1);
    otherGrid->setColumnStretch(1, 1);
    otherLayout->addLayout(otherGrid);
    otherLayout->addStretch(1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        this);
    buttons->setObjectName(QStringLiteral("DialogButtons"));
    m_applyButton = buttons->button(QDialogButtonBox::Apply);
    if (m_applyButton) {
        m_applyButton->setEnabled(false);
    }
    QPushButton *restore = buttons->addButton(
        tr("Restore Defaults"), QDialogButtonBox::ResetRole);
    connect(restore, &QPushButton::clicked, this,
            &PreferencesDialog::restoreDefaults);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &PreferencesDialog::apply);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (apply()) {
            accept();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connectChangeTracking();
    updateMasjidScreens();
    populate(configObject(m_core));
    fitDialogToPages(this, m_stack, buttons, 900);
}

void PreferencesDialog::setTabIndex(int index) {
    if (m_nav) {
        m_nav->setCurrentRow(qBound(0, index, m_nav->count() - 1));
    }
}

void PreferencesDialog::reload() {
    updateMasjidScreens();
    populate(configObject(m_core));
}

void PreferencesDialog::markDirty() {
    if (m_applyButton) {
        m_applyButton->setEnabled(true);
    }
}

void PreferencesDialog::connectChangeTracking() {
    for (QLineEdit *edit : findChildren<QLineEdit *>()) {
        connect(edit, &QLineEdit::textEdited, this,
                &PreferencesDialog::markDirty);
    }
    for (QComboBox *combo : findChildren<QComboBox *>()) {
        connect(combo, &QComboBox::currentIndexChanged, this,
                &PreferencesDialog::markDirty);
    }
    for (QSpinBox *spin : findChildren<QSpinBox *>()) {
        connect(spin, &QSpinBox::valueChanged, this,
                &PreferencesDialog::markDirty);
    }
    for (QCheckBox *check : findChildren<QCheckBox *>()) {
        connect(check, &QCheckBox::toggled, this,
                &PreferencesDialog::markDirty);
    }
    for (QSlider *slider : findChildren<QSlider *>()) {
        connect(slider, &QSlider::valueChanged, this,
                &PreferencesDialog::markDirty);
    }
    for (QTimeEdit *time : findChildren<QTimeEdit *>()) {
        connect(time, &QTimeEdit::timeChanged, this,
                &PreferencesDialog::markDirty);
    }
}

void PreferencesDialog::updateAudioRow(int index) {
    if (!m_audioMode[index] || !m_audioStack[index]) {
        return;
    }
    const int mode = m_audioMode[index]->currentData().toInt();
    m_audioStack[index]->setCurrentIndex(
        qBound(0, mode, m_audioStack[index]->count() - 1));
}

void PreferencesDialog::updateStartupRow() {
    if (!m_startupMode || !m_startupStack) {
        return;
    }
    const int mode = m_startupMode->currentData().toInt();
    m_startupStack->setCurrentIndex(qBound(0, mode, m_startupStack->count() - 1));
}

void PreferencesDialog::updateReminderAudioRow() {
    const int mode = m_reminderAudioMode->currentData().toInt();
    m_reminderFile->setEnabled(mode == 4);
    m_reminderFolder->setEnabled(mode == 5);
    if (m_reminderFileRow) {
        m_reminderFileRow->setVisible(mode == 4);
    }
    if (m_reminderFolderRow) {
        m_reminderFolderRow->setVisible(mode == 5);
    }
    if (m_reminderFileLabel) {
        m_reminderFileLabel->setVisible(mode == 4);
    }
    if (m_reminderFolderLabel) {
        m_reminderFolderLabel->setVisible(mode == 5);
    }
}

void PreferencesDialog::updateMasjidScreens() {
    if (!m_masjidScreen) {
        return;
    }
    const int current = m_masjidScreen->currentData().toInt();
    m_masjidScreen->clear();
    m_masjidScreen->addItem(tr("Primary screen"), 0);
    m_masjidScreen->addItem(tr("All screens"), -1);
    const auto screens = QGuiApplication::screens();
    for (int index = 0; index < screens.size(); ++index) {
        m_masjidScreen->addItem(
            tr("%1 (%2)").arg(screens.at(index)->name(),
                              screens.at(index)->geometry().size().width()
                                  ? tr("%1×%2")
                                        .arg(screens.at(index)->geometry().width())
                                        .arg(screens.at(index)->geometry().height())
                                  : QString()),
            index + 1);
    }
    const int found = m_masjidScreen->findData(current);
    m_masjidScreen->setCurrentIndex(found >= 0 ? found : 0);
}

void PreferencesDialog::browseFile(QLineEdit *target) {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Audio File"), target->text(),
        tr("WAV files (*.wav);;All files (*)"));
    if (!path.isEmpty()) {
        target->setText(path);
    }
}

void PreferencesDialog::browseFolder(QLineEdit *target) {
    const QString path = QFileDialog::getExistingDirectory(
        this, tr("Select Audio Folder"), target->text());
    if (!path.isEmpty()) {
        target->setText(path);
    }
}

void PreferencesDialog::previewSound(const QString &name) {
    if (name.isEmpty()) {
        return;
    }
    const bool ok = name == QStringLiteral("<random>")
                        ? namaz_core_play_random_athan() != 0
                        : namaz_core_play_sound(name.toUtf8().constData()) != 0;
    if (!ok) {
        QMessageBox::warning(this, tr("Audio"),
                             tr("Audio file not found: %1").arg(name));
    }
}

void PreferencesDialog::previewFile(const QLineEdit *edit) {
    const QString path = edit->text();
    if (path.isEmpty()) {
        return;
    }
    if (!namaz_core_play_path(path.toUtf8().constData())) {
        QMessageBox::warning(this, tr("Audio"),
                             tr("Audio file not found:\n%1").arg(path));
    }
}

void PreferencesDialog::previewFolder(const QLineEdit *edit) {
    const QString path = edit->text();
    if (path.isEmpty()) {
        return;
    }
    if (!namaz_core_play_folder(path.toUtf8().constData())) {
        QMessageBox::warning(this, tr("Audio"),
                             tr("No WAV files found in:\n%1").arg(path));
    }
}

void PreferencesDialog::populate(const QJsonObject &config) {
    m_athanEnabled->setChecked(config.value(QStringLiteral("athan_enabled")).toBool(true));
    m_notifications->setChecked(
        config.value(QStringLiteral("notifications_enabled")).toBool(true));
    m_stopOnPrayer->setChecked(
        config.value(QStringLiteral("stop_audio_on_prayer")).toBool(false));
    const int volume = config.value(QStringLiteral("volume")).toInt(90);
    m_volume->setValue(volume);
    m_volumeValue->setText(QStringLiteral("%1%").arg(volume));

    const QJsonArray sounds = config.value(QStringLiteral("sounds")).toArray();
    const QJsonArray modes = config.value(QStringLiteral("athan_modes")).toArray();
    const QJsonArray files = config.value(QStringLiteral("sound_files")).toArray();
    const QJsonArray folders = config.value(QStringLiteral("athan_folders")).toArray();
    for (int index = 0; index < 6; ++index) {
        const bool shuruq = index == 1;
        const QString sound = shuruq
                                  ? QString()
                                  : (index < sounds.size() ? sounds.at(index).toString()
                                                          : QString());
        int soundIndex = m_sounds[index]->findData(sound);
        if (soundIndex < 0 && !sound.isEmpty()) {
            m_sounds[index]->addItem(sound, sound);
            soundIndex = m_sounds[index]->count() - 1;
        }
        m_sounds[index]->setCurrentIndex(qMax(0, soundIndex));
        m_soundFile[index]->setText(
            shuruq ? QString()
                   : (index < files.size() ? files.at(index).toString() : QString()));
        m_soundFolder[index]->setText(
            shuruq ? QString()
                   : (index < folders.size() ? folders.at(index).toString() : QString()));
        const int mode = shuruq ? 0 : (index < modes.size() ? modes.at(index).toInt() : 1);
        m_audioMode[index]->setCurrentIndex(
            qMax(0, m_audioMode[index]->findData(mode)));
        updateAudioRow(index);
    }

    const int startupMode = config.value(QStringLiteral("startup_mode")).toInt(1);
    m_startupMode->setCurrentIndex(qMax(0, m_startupMode->findData(startupMode)));
    m_startupFile->setText(config.value(QStringLiteral("startup_file")).toString());
    m_startupFolder->setText(config.value(QStringLiteral("startup_folder")).toString());
    updateStartupRow();

    const QJsonArray before = config.value(QStringLiteral("reminders_before")).toArray();
    const QJsonArray after = config.value(QStringLiteral("reminders_after")).toArray();
    const QJsonArray beforeOn = config.value(QStringLiteral("reminders_before_on")).toArray();
    const QJsonArray afterOn = config.value(QStringLiteral("reminders_after_on")).toArray();
    const QJsonArray fixed = config.value(QStringLiteral("reminders_before_fixed")).toArray();
    const QJsonArray fixedOn = config.value(QStringLiteral("reminders_fixed_on")).toArray();
    const QJsonArray iqama = config.value(QStringLiteral("iqama")).toArray();
    const QJsonArray iqamaOn = config.value(QStringLiteral("iqama_on")).toArray();
    for (int index = 0; index < 6; ++index) {
        m_beforeOn[index]->setChecked(index < beforeOn.size() && beforeOn.at(index).toBool());
        m_beforeMinutes[index]->setValue(index < before.size() ? before.at(index).toInt() : 0);
        m_afterOn[index]->setChecked(index < afterOn.size() && afterOn.at(index).toBool());
        m_afterMinutes[index]->setValue(index < after.size() ? after.at(index).toInt() : 0);
        m_fixedOn[index]->setChecked(index < fixedOn.size() && fixedOn.at(index).toBool());
        const int fixedMinutes = index < fixed.size() ? fixed.at(index).toInt() : 0;
        m_fixedTime[index]->setTime(QTime((fixedMinutes / 60) % 24, fixedMinutes % 60));
        const bool shuruq = index == 1;
        m_iqamaOn[index]->setChecked(
            !shuruq && index < iqamaOn.size() && iqamaOn.at(index).toBool());
        m_iqamaMinutes[index]->setValue(
            shuruq ? 0 : (index < iqama.size() ? iqama.at(index).toInt() : 0));
    }

    m_reminderNotification->setChecked(
        config.value(QStringLiteral("reminder_visual_notification")).toBool(true));
    m_reminderMain->setChecked(
        config.value(QStringLiteral("reminder_visual_main")).toBool(false));
    m_reminderFlash->setChecked(
        config.value(QStringLiteral("reminder_visual_flash")).toBool(false));
    const int reminderMode = config.value(QStringLiteral("reminder_audio_mode")).toInt(0);
    m_reminderAudioMode->setCurrentIndex(
        qMax(0, m_reminderAudioMode->findData(reminderMode)));
    m_reminderFile->setText(config.value(QStringLiteral("reminder_sound")).toString());
    m_reminderFolder->setText(config.value(QStringLiteral("reminder_folder")).toString());
    updateReminderAudioRow();

    m_alwaysOnTop->setChecked(config.value(QStringLiteral("always_on_top")).toBool());
    m_hideOnClose->setChecked(config.value(QStringLiteral("hide_on_close")).toBool());
    m_launchMinimized->setChecked(
        config.value(QStringLiteral("launch_minimized")).toBool());
    m_launchOnStartup->setChecked(
        config.value(QStringLiteral("launch_on_startup")).toBool());
    m_athanVisualMain->setChecked(
        config.value(QStringLiteral("athan_visual_main")).toBool(false));
    m_athanVisualFlash->setChecked(
        config.value(QStringLiteral("athan_visual_flash")).toBool(false));
    m_hijriCalendar->setChecked(
        config.value(QStringLiteral("hijri_calendar")).toBool(true));
    m_hijriOffset->setValue(config.value(QStringLiteral("hijri_offset")).toInt());
    m_timeFormat->setCurrentIndex(qMax(
        0, m_timeFormat->findData(
               config.value(QStringLiteral("time_format")).toInt(0))));
    m_showQiblah->setChecked(config.value(QStringLiteral("show_qiblah")).toBool(true));
    m_qiblaClockwise->setChecked(
        config.value(QStringLiteral("qibla_clockwise")).toBool(true));
    m_lockOnPray->setChecked(
        config.value(QStringLiteral("lock_on_pray_now")).toBool(false));
    m_screensaverOnPray->setChecked(
        config.value(QStringLiteral("screensaver_on_pray_now")).toBool(false));
    m_showSplash->setChecked(config.value(QStringLiteral("show_splash")).toBool(false));
    m_dbusEnabled->setChecked(config.value(QStringLiteral("dbus_enabled")).toBool(false));
    m_dbusInterval->setValue(config.value(QStringLiteral("dbus_interval")).toInt(60));
    m_masjidScreen->setCurrentIndex(qMax(
        0, m_masjidScreen->findData(
               config.value(QStringLiteral("masjid_screen")).toInt(0))));
    const QString locale = config.value(QStringLiteral("locale")).toString();
    m_language->setCurrentIndex(qMax(0, m_language->findData(
        locale.isEmpty() ? QStringLiteral("system") : locale)));
    updateMasjidScreens();
    if (m_applyButton) {
        m_applyButton->setEnabled(false);
    }
}

void PreferencesDialog::restoreDefaults() {
    populate(defaultConfigObject());
    // Restoring defaults is a change that still has to be applied.
    if (m_applyButton) {
        m_applyButton->setEnabled(true);
    }
}

bool PreferencesDialog::apply() {
    QJsonObject config;
    config.insert(QStringLiteral("athan_enabled"), m_athanEnabled->isChecked());
    config.insert(QStringLiteral("notifications_enabled"), m_notifications->isChecked());
    config.insert(QStringLiteral("stop_audio_on_prayer"), m_stopOnPrayer->isChecked());
    config.insert(QStringLiteral("volume"), m_volume->value());

    QJsonArray sounds;
    QJsonArray modes;
    QJsonArray files;
    QJsonArray folders;
    for (int index = 0; index < 6; ++index) {
        const bool shuruq = index == 1;
        sounds.append(shuruq ? QString() : m_sounds[index]->currentData().toString());
        modes.append(shuruq ? 0 : m_audioMode[index]->currentData().toInt());
        files.append(shuruq ? QString() : m_soundFile[index]->text());
        folders.append(shuruq ? QString() : m_soundFolder[index]->text());
    }
    config.insert(QStringLiteral("sounds"), sounds);
    config.insert(QStringLiteral("athan_modes"), modes);
    config.insert(QStringLiteral("sound_files"), files);
    config.insert(QStringLiteral("athan_folders"), folders);

    config.insert(QStringLiteral("startup_mode"), m_startupMode->currentData().toInt());
    config.insert(QStringLiteral("startup_file"), m_startupFile->text());
    config.insert(QStringLiteral("startup_folder"), m_startupFolder->text());

    QJsonArray before;
    QJsonArray after;
    QJsonArray beforeOn;
    QJsonArray afterOn;
    QJsonArray fixed;
    QJsonArray fixedOn;
    QJsonArray iqama;
    QJsonArray iqamaOn;
    for (int index = 0; index < 6; ++index) {
        before.append(m_beforeMinutes[index]->value());
        after.append(m_afterMinutes[index]->value());
        beforeOn.append(m_beforeOn[index]->isChecked());
        afterOn.append(m_afterOn[index]->isChecked());
        fixed.append(m_fixedTime[index]->time().msecsSinceStartOfDay() / 60000);
        fixedOn.append(m_fixedOn[index]->isChecked());
        const bool shuruq = index == 1;
        iqama.append(shuruq ? 0 : m_iqamaMinutes[index]->value());
        iqamaOn.append(!shuruq && m_iqamaOn[index]->isChecked());
    }
    config.insert(QStringLiteral("reminders_before"), before);
    config.insert(QStringLiteral("reminders_after"), after);
    config.insert(QStringLiteral("reminders_before_on"), beforeOn);
    config.insert(QStringLiteral("reminders_after_on"), afterOn);
    config.insert(QStringLiteral("reminders_before_fixed"), fixed);
    config.insert(QStringLiteral("reminders_fixed_on"), fixedOn);
    config.insert(QStringLiteral("iqama"), iqama);
    config.insert(QStringLiteral("iqama_on"), iqamaOn);

    config.insert(QStringLiteral("reminder_visual_notification"),
                  m_reminderNotification->isChecked());
    config.insert(QStringLiteral("reminder_visual_main"), m_reminderMain->isChecked());
    config.insert(QStringLiteral("reminder_visual_flash"), m_reminderFlash->isChecked());
    config.insert(QStringLiteral("reminder_audio_mode"),
                  m_reminderAudioMode->currentData().toInt());
    config.insert(QStringLiteral("reminder_sound"), m_reminderFile->text());
    config.insert(QStringLiteral("reminder_folder"), m_reminderFolder->text());

    config.insert(QStringLiteral("always_on_top"), m_alwaysOnTop->isChecked());
    config.insert(QStringLiteral("hide_on_close"), m_hideOnClose->isChecked());
    config.insert(QStringLiteral("launch_minimized"), m_launchMinimized->isChecked());
    config.insert(QStringLiteral("launch_on_startup"), m_launchOnStartup->isChecked());
    config.insert(QStringLiteral("athan_visual_main"), m_athanVisualMain->isChecked());
    config.insert(QStringLiteral("athan_visual_flash"), m_athanVisualFlash->isChecked());
    config.insert(QStringLiteral("hijri_calendar"), m_hijriCalendar->isChecked());
    config.insert(QStringLiteral("hijri_offset"), m_hijriOffset->value());
    config.insert(QStringLiteral("time_format"), m_timeFormat->currentData().toInt());
    config.insert(QStringLiteral("show_qiblah"), m_showQiblah->isChecked());
    config.insert(QStringLiteral("qibla_clockwise"), m_qiblaClockwise->isChecked());
    config.insert(QStringLiteral("lock_on_pray_now"), m_lockOnPray->isChecked());
    config.insert(QStringLiteral("screensaver_on_pray_now"), m_screensaverOnPray->isChecked());
    config.insert(QStringLiteral("show_splash"), m_showSplash->isChecked());
    config.insert(QStringLiteral("dbus_enabled"), m_dbusEnabled->isChecked());
    config.insert(QStringLiteral("dbus_interval"), m_dbusInterval->value());
    config.insert(QStringLiteral("masjid_screen"), m_masjidScreen->currentData().toInt());
    config.insert(QStringLiteral("locale"), m_language->currentData().toString());

    const QByteArray json = QJsonDocument(config).toJson(QJsonDocument::Compact);
    if (namaz_core_apply_preferences_json(m_core, json.constData())) {
        if (m_applyButton) {
            m_applyButton->setEnabled(false);
        }
        emit applied();
        return true;
    }
    QMessageBox::warning(
        this, tr("Preferences"),
        tr("Could not save the configuration file. Check the permissions of "
           "the configuration directory."));
    return false;
}
