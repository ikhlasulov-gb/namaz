#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QJsonObject>
#include <QString>

class QFormLayout;
class QGridLayout;
class QLabel;
class QTableWidget;
class QWidget;

namespace Ui {

// Resolve a Breeze icon by name, falling back to the application style.
QIcon themeIcon(const QString &name, const QString &fallback = QString());

// Application icon: theme icon, bundled resource, then style fallback.
QIcon appIcon();

// Scale a font relative to the system font.  Never hard-code point sizes.
QFont scaledFont(const QFont &base, qreal factor, bool bold = false);

// Like scaledFont(), but with tabular figures so a changing clock does not
// shift the surrounding layout.
QFont tabularFont(const QFont &base, qreal factor, bool bold = false);

// A small, secondary caption that follows the system palette.
void styleCaption(QLabel *label);

// Locale-aware time formatting.  Negative seconds mean "not available".
QString formatTime(int seconds, bool withSeconds = false);
QString formatCountdown(int seconds);

// Stable English source strings translated with the current UI language.
QString prayerName(int index);
QString hijriMonthName(int month);
QString methodName(int method);
QString methodShortName(int method);

// Locale-aware Gregorian + Hijri date line from the core `date` object.
QString formatDateLine(const QJsonObject &date);
// The Hijri part alone (empty when Hijri display is disabled or unavailable).
QString formatHijriLine(const QJsonObject &date);

// A translucent accent used only as a row tint, never as text colour.
QColor tintColor(const QColor &base, int alpha = 38);

// Mix two opaque colours; amount 0 returns `first`, 1 returns `second`.
// Useful for deriving subtle zebra/outline colours from palette roles.
QColor mixColor(const QColor &first, const QColor &second, qreal amount);

// One spacing scale for the whole application (logical pixels):
// 4 tight related elements, 8 inside a group, 12 between neighbours,
// 16 inside main sections, 24 between independent blocks.
int spacingTight();
int pageMargin();
int groupSpacing();
int controlSpacing();
int sectionSpacing();
int blockSpacing();

// Compact form/grid defaults.  No QSS: only layout metrics are touched.
void compactForm(QFormLayout *layout);
void compactGrid(QGridLayout *layout);

// Configure a table for compact, HIG-friendly rows.  The row height follows
// the system font instead of stretching to fill the viewport.
void compactTable(QTableWidget *table, int rowPadding = 8);

// Give an input a KDE-like preferred width instead of letting it grow to the
// whole dialog.  Values are in device-independent pixels.
void setFieldWidth(QWidget *widget, int width);

}  // namespace Ui
