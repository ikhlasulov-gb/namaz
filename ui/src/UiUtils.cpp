#include "UiUtils.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QStyle>
#include <QTableWidget>
#include <QTime>
#include <QWidget>

namespace Ui {

QIcon appIcon() {
    QIcon icon = QIcon::fromTheme(QStringLiteral("io.github.ikhlasulov_gb.Namaz"));
    if (!icon.isNull()) {
        return icon;
    }
    icon = QIcon(QStringLiteral(":/assets/skins/app-icon.png"));
    if (!icon.isNull()) {
        return icon;
    }
    return QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
}

QIcon themeIcon(const QString &name, const QString &fallback) {
    QIcon icon = QIcon::fromTheme(name);
    if (!icon.isNull()) {
        return icon;
    }
    if (!fallback.isEmpty()) {
        icon = QIcon::fromTheme(fallback);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
}

QFont scaledFont(const QFont &base, qreal factor, bool bold) {
    QFont font = base;
    qreal pointSize = font.pointSizeF();
    if (pointSize <= 0.0) {
        pointSize = QFontInfo(base).pointSizeF();
    }
    if (pointSize > 0.0) {
        font.setPointSizeF(qMax(6.5, pointSize * factor));
    }
    if (bold) {
        font.setWeight(QFont::DemiBold);
    }
    return font;
}

QFont tabularFont(const QFont &base, qreal factor, bool bold) {
    QFont font = scaledFont(base, factor, bold);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    // OpenType feature "tnum": equal-width digits.  Fonts without it simply
    // keep their proportional digits.
    font.setFeature(QFont::Tag("tnum"), 1);
#endif
    return font;
}

void styleCaption(QLabel *label) {
    if (!label) {
        return;
    }
    label->setForegroundRole(QPalette::PlaceholderText);
    QFont font = label->font();
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
}

QString formatTime(int seconds, bool withSeconds) {
    if (seconds < 0) {
        return QStringLiteral("—");
    }
    const int hour = (seconds / 3600) % 24;
    const int minute = (seconds % 3600) / 60;
    const int second = seconds % 60;
    const QTime time(hour, minute, second);
    const QLocale locale = QLocale();
    if (!withSeconds) {
        return locale.toString(time, QLocale::ShortFormat);
    }
    QString pattern = locale.timeFormat(QLocale::ShortFormat);
    if (!pattern.contains(QLatin1String("ss"))) {
        int marker = -1;
        for (int index = 0; index < pattern.size(); ++index) {
            const QChar character = pattern.at(index);
            if (character == QLatin1Char('A') || character == QLatin1Char('a')
                || character == QLatin1Char('P') || character == QLatin1Char('p')) {
                marker = index;
                break;
            }
        }
        int position = marker >= 0 ? marker : pattern.size();
        while (position > 0 && pattern.at(position - 1).isSpace()) {
            --position;
        }
        pattern.insert(position, QStringLiteral(":ss"));
    }
    return locale.toString(time, pattern);
}

QString formatCountdown(int seconds) {
    if (seconds < 0) {
        return QString();
    }
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int secs = seconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QString prayerName(int index) {
    static const char *const names[] = {
        QT_TRANSLATE_NOOP("DayView", "Fajr"),
        QT_TRANSLATE_NOOP("DayView", "Shuruq"),
        QT_TRANSLATE_NOOP("DayView", "Dhuhr"),
        QT_TRANSLATE_NOOP("DayView", "Asr"),
        QT_TRANSLATE_NOOP("DayView", "Maghrib"),
        QT_TRANSLATE_NOOP("DayView", "Ishaa"),
    };
    if (index < 0 || index >= 6) {
        return QString();
    }
    return QCoreApplication::translate("DayView", names[index]);
}

QString hijriMonthName(int month) {
    static const char *const names[] = {
        QT_TRANSLATE_NOOP("Hijri", "Muharram"),
        QT_TRANSLATE_NOOP("Hijri", "Safar"),
        QT_TRANSLATE_NOOP("Hijri", "Rabi' I"),
        QT_TRANSLATE_NOOP("Hijri", "Rabi' II"),
        QT_TRANSLATE_NOOP("Hijri", "Jumada I"),
        QT_TRANSLATE_NOOP("Hijri", "Jumada II"),
        QT_TRANSLATE_NOOP("Hijri", "Rajab"),
        QT_TRANSLATE_NOOP("Hijri", "Sha'ban"),
        QT_TRANSLATE_NOOP("Hijri", "Ramadan"),
        QT_TRANSLATE_NOOP("Hijri", "Shawwal"),
        QT_TRANSLATE_NOOP("Hijri", "Dhu al-Qi'dah"),
        QT_TRANSLATE_NOOP("Hijri", "Dhu al-Hijjah"),
    };
    if (month < 1 || month > 12) {
        return QString();
    }
    return QCoreApplication::translate("Hijri", names[month - 1]);
}

QString methodName(int method) {
    // Reuse the combo labels from Prayer Settings so every language gets the
    // existing translations instead of a second, untranslated copy.
    switch (method) {
    case 1:
        return QCoreApplication::translate("PrayerSettingsDialog",
                                           "Egyptian General Authority of Survey");
    case 2:
        return QCoreApplication::translate(
            "PrayerSettingsDialog", "University of Islamic Sciences, Karachi");
    case 4:
        return QCoreApplication::translate(
            "PrayerSettingsDialog", "Islamic Society of North America (ISNA)");
    case 5:
        return QCoreApplication::translate("PrayerSettingsDialog",
                                           "Muslim World League");
    case 6:
        return QCoreApplication::translate("PrayerSettingsDialog", "Umm al-Qura");
    default:
        return QCoreApplication::translate("PrayerSettingsDialog",
                                           "Custom twilight angles");
    }
}

QString methodShortName(int method) {
    switch (method) {
    case 1:
        return QCoreApplication::translate("MainWindow", "Egyptian");
    case 2:
        return QCoreApplication::translate("MainWindow", "Karachi");
    case 4:
        return QStringLiteral("ISNA");
    case 5:
        return QStringLiteral("MWL");
    case 6:
        return QCoreApplication::translate("MainWindow", "Umm al-Qura");
    default:
        return QCoreApplication::translate("MainWindow", "Custom");
    }
}

QString formatDateLine(const QJsonObject &date) {
    const QDate selected =
        QDate::fromString(date.value(QStringLiteral("iso")).toString(), Qt::ISODate);
    QString line = selected.isValid()
                       ? QLocale().toString(selected, QLocale::LongFormat)
                       : date.value(QStringLiteral("line")).toString();
    const QString hijri = formatHijriLine(date);
    if (hijri.isEmpty()) {
        return line;
    }
    line += QStringLiteral(" / ") + hijri;
    return line;
}

QString formatHijriLine(const QJsonObject &date) {
    if (!date.value(QStringLiteral("showHijri")).toBool()) {
        return {};
    }
    const int hijriMonth = date.value(QStringLiteral("hijriMonth")).toInt();
    const int hijriDay = date.value(QStringLiteral("hijriDay")).toInt();
    const int hijriYear = date.value(QStringLiteral("hijriYear")).toInt();
    if (hijriMonth < 1 || hijriMonth > 12 || hijriDay < 1 || hijriYear < 1) {
        return {};
    }
    return QStringLiteral("%1 %2, %3")
        .arg(hijriMonthName(hijriMonth))
        .arg(hijriDay)
        .arg(hijriYear);
}

QColor tintColor(const QColor &base, int alpha) {
    QColor color = base;
    color.setAlpha(alpha);
    return color;
}

int spacingTight() {
    return 4;
}

int pageMargin() {
    return qMax(16, QApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin));
}

int groupSpacing() {
    return qMax(8, QApplication::style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
}

int controlSpacing() {
    return qMax(8, QApplication::style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
}

int sectionSpacing() {
    return 16;
}

int blockSpacing() {
    return 24;
}

void compactForm(QFormLayout *layout) {
    if (!layout) {
        return;
    }
    layout->setContentsMargins(pageMargin(), pageMargin(), pageMargin(), pageMargin());
    layout->setHorizontalSpacing(controlSpacing() + 4);
    layout->setVerticalSpacing(groupSpacing());
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

void compactGrid(QGridLayout *layout) {
    if (!layout) {
        return;
    }
    layout->setContentsMargins(pageMargin(), pageMargin(), pageMargin(), pageMargin());
    layout->setHorizontalSpacing(controlSpacing() + 4);
    layout->setVerticalSpacing(groupSpacing());
}

void compactTable(QTableWidget *table, int rowPadding) {
    if (!table) {
        return;
    }
    const int rowHeight = table->fontMetrics().height() + qMax(4, rowPadding);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(rowHeight);
    table->verticalHeader()->setMinimumSectionSize(rowHeight);
    table->horizontalHeader()->setMinimumSectionSize(24);
    table->setWordWrap(false);
}

void setFieldWidth(QWidget *widget, int width) {
    if (!widget) {
        return;
    }
    widget->setMaximumWidth(width);
    widget->setSizePolicy(QSizePolicy::Preferred, widget->sizePolicy().verticalPolicy());
}

}  // namespace Ui
