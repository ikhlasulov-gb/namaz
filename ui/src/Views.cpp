#include "Views.h"

#include "UiUtils.h"

#include <QBoxLayout>
#include <QCalendarWidget>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QProgressBar>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWindow>

#include <cmath>

namespace {

// Stable English labels for the "Raka'as" column; translated in DayView.
const char *const kPrayerInfo[] = {
    QT_TRANSLATE_NOOP("DayView", "2 Raka'as"),
    QT_TRANSLATE_NOOP("DayView", "Sunrise"),
    QT_TRANSLATE_NOOP("DayView", "4 Raka'as"),
    QT_TRANSLATE_NOOP("DayView", "4 Raka'as"),
    QT_TRANSLATE_NOOP("DayView", "3 Raka'as"),
    QT_TRANSLATE_NOOP("DayView", "4 Raka'as"),
};

QTableWidgetItem *makeItem(const QString &text, Qt::Alignment alignment) {
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(alignment);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

int rowHeightFor(const QFont &font, int padding) {
    return QFontMetrics(font).height() + qMax(4, padding);
}

}  // namespace

// ── QiblahCompass ───────────────────────────────────────────────────────────

QiblahCompass::QiblahCompass(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("QiblahCompass"));
    setAccessibleName(DayView::tr("Qiblah compass"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(minimumSizeHint());
}

QSize QiblahCompass::sizeHint() const {
    return QSize(200, 200);
}

QSize QiblahCompass::minimumSizeHint() const {
    return QSize(180, 180);
}

void QiblahCompass::setBearing(qreal bearing) {
    qreal normalized = std::fmod(bearing, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    m_bearing = normalized;
    setAccessibleDescription(
        DayView::tr("%1° from north").arg(normalized, 0, 'f', 2));
    setToolTip(DayView::tr("Qiblah direction: %1°").arg(normalized, 0, 'f', 2));
    update();
}

void QiblahCompass::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int side = qMin(width(), height());
    const qreal margin = qMax<qreal>(14.0, side * 0.08);
    const qreal radius = side / 2.0 - margin;
    if (radius < 24.0) {
        return;
    }
    const QPointF center(width() / 2.0, height() / 2.0);
    const QPalette pal = palette();
    const QColor text = pal.color(QPalette::WindowText);
    // QPalette::Mid merges into the background in dark colour schemes (Breeze
    // Dark is roughly 2.6:1).  Blend the text colour instead: the ring stays
    // above 4:1 in the dark theme and above 3:1 in the light one.
    QColor ring = text;
    ring.setAlpha(140);
    const QColor accent = pal.color(QPalette::Highlight);

    painter.setPen(QPen(ring, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);

    for (int degree = 0; degree < 360; degree += 10) {
        const bool major = degree % 90 == 0;
        const bool medium = degree % 45 == 0;
        const qreal length = major ? 11.0 : (medium ? 7.5 : 4.0);
        painter.save();
        painter.translate(center);
        painter.rotate(degree);
        painter.setPen(QPen(major ? text : ring, major ? 1.6 : 1.0));
        painter.drawLine(QPointF(0, -radius + 1.5),
                         QPointF(0, -radius + 1.5 + length));
        painter.restore();
    }

    const QFont labelFont = Ui::scaledFont(font(), 0.9, false);
    painter.setFont(labelFont);
    painter.setPen(text);
    const QFontMetricsF metrics(labelFont);
    const qreal labelRadius = radius + margin * 0.52;
    struct Cardinal {
        const char *text;
        qreal degrees;
    };
    static const Cardinal cardinals[] = {{"N", 0.0}, {"E", 90.0},
                                         {"S", 180.0}, {"W", 270.0}};
    for (const Cardinal &cardinal : cardinals) {
        const QString label = QString::fromLatin1(cardinal.text);
        const qreal width = metrics.horizontalAdvance(label);
        const qreal height = metrics.height();
        painter.save();
        painter.translate(center);
        painter.rotate(cardinal.degrees);
        painter.translate(0, -labelRadius);
        painter.rotate(-cardinal.degrees);
        painter.drawText(QRectF(-width / 2.0, -height / 2.0, width, height),
                         Qt::AlignCenter, label);
        painter.restore();
    }

    const qreal tip = radius * 0.80;
    const qreal half = qMax<qreal>(3.0, radius * 0.055);
    const qreal tail = radius * 0.20;
    painter.save();
    painter.translate(center);
    painter.rotate(m_bearing);
    QPolygonF needle;
    needle << QPointF(0, -tip) << QPointF(half, 0) << QPointF(0, tail)
           << QPointF(-half, 0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawPolygon(needle);
    painter.restore();

    const qreal hub = qMax<qreal>(2.0, radius * 0.018);
    painter.setPen(Qt::NoPen);
    painter.setBrush(text);
    painter.drawEllipse(center, hub, hub);
}

// ── CurrentTimeCard ────────────────────────────────────────────────────────

CurrentTimeCard::CurrentTimeCard(QWidget *parent)
    : QGroupBox(DayView::tr("CURRENT TIME"), parent) {
    setObjectName(QStringLiteral("CurrentTimeCard"));
    setAlignment(Qt::AlignLeft);
    setAccessibleName(DayView::tr("Current time"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::groupSpacing() + 4,
                               Ui::pageMargin(), Ui::groupSpacing() + 4);
    layout->setSpacing(Ui::spacingTight());

    m_timeLabel = new QLabel(QStringLiteral("--:--:--"), this);
    m_timeLabel->setObjectName(QStringLiteral("CurrentTimeValue"));
    m_timeLabel->setFont(Ui::tabularFont(font(), 2.4, true));
    m_timeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    const QFontMetrics metrics(m_timeLabel->font());
    m_timeLabel->setMinimumWidth(
        qMax(metrics.horizontalAdvance(Ui::formatTime(0, true)),
             metrics.horizontalAdvance(
                 Ui::formatTime(13 * 3600 + 59 * 60 + 59, true)))
        + 6);

    m_prayerLabel = new QLabel(this);
    m_prayerLabel->setObjectName(QStringLiteral("CurrentPrayerLabel"));
    m_prayerLabel->setFont(Ui::scaledFont(font(), 0.95, false));
    m_prayerLabel->setForegroundRole(QPalette::PlaceholderText);
    m_prayerLabel->setWordWrap(true);
    m_prayerLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Clock on the left, current prayer on the right: the card uses its whole
    // width instead of leaving the right half empty.
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(Ui::sectionSpacing());
    row->addWidget(m_timeLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    row->addStretch(1);
    row->addWidget(m_prayerLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    layout->addStretch(1);
    layout->addLayout(row);
    layout->addStretch(1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CurrentTimeCard::setState(const QJsonObject &state) {
    const int seconds = state.value(QStringLiteral("now"))
                            .toObject()
                            .value(QStringLiteral("seconds"))
                            .toInt(-1);
    m_timeLabel->setText(seconds >= 0 ? Ui::formatTime(seconds, true)
                                      : QStringLiteral("--:--:--"));
    const int index = state.value(QStringLiteral("current"))
                          .toObject()
                          .value(QStringLiteral("index"))
                          .toInt(-1);
    const QString name = Ui::prayerName(index);
    m_prayerLabel->setText(
        name.isEmpty()
            ? DayView::tr("Current prayer: —")
            : DayView::tr("Current prayer: %1").arg(name));
}

// ── NextPrayerCard ─────────────────────────────────────────────────────────

NextPrayerCard::NextPrayerCard(QWidget *parent)
    : QGroupBox(DayView::tr("NEXT PRAYER"), parent) {
    setObjectName(QStringLiteral("NextPrayerCard"));
    setAlignment(Qt::AlignLeft);
    setAccessibleName(DayView::tr("Next prayer"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::groupSpacing() + 4,
                               Ui::pageMargin(), Ui::groupSpacing() + 4);
    layout->setSpacing(Ui::spacingTight());

    m_timeLabel = new QLabel(QStringLiteral("—"), this);
    m_timeLabel->setObjectName(QStringLiteral("NextPrayerTime"));
    m_timeLabel->setFont(Ui::tabularFont(font(), 1.9, true));
    const QFontMetrics metrics(m_timeLabel->font());
    m_timeLabel->setMinimumWidth(
        metrics.horizontalAdvance(Ui::formatTime(23 * 3600 + 59 * 60)) + 6);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("NextPrayerName"));
    m_nameLabel->setFont(Ui::scaledFont(font(), 1.3, true));

    m_countdownLabel = new QLabel(this);
    m_countdownLabel->setObjectName(QStringLiteral("NextPrayerCountdown"));
    m_countdownLabel->setFont(Ui::scaledFont(font(), 0.95, false));
    m_countdownLabel->setForegroundRole(QPalette::PlaceholderText);
    m_countdownLabel->setWordWrap(true);
    m_countdownLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("NextPrayerProgress"));
    m_progress->setRange(0, 1000);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_progress->setAccessibleName(DayView::tr("Progress until the next prayer"));

    // Time on the left, countdown on the right; the progress bar anchors the
    // card at the bottom and spans the full width.
    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(Ui::sectionSpacing());
    topRow->addWidget(m_timeLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    topRow->addStretch(1);
    topRow->addWidget(m_countdownLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    layout->addLayout(topRow);
    layout->addWidget(m_nameLabel);
    layout->addStretch(1);
    layout->addWidget(m_progress);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void NextPrayerCard::setState(const QJsonObject &state) {
    const QJsonObject next = state.value(QStringLiteral("next")).toObject();
    const int index = next.value(QStringLiteral("index")).toInt(-1);
    const int seconds = next.value(QStringLiteral("seconds")).toInt(-1);
    const int remaining = next.value(QStringLiteral("remaining")).toInt(-1);
    const QString name = Ui::prayerName(index);
    const bool hasNext = index >= 0 && seconds >= 0;

    m_timeLabel->setText(hasNext ? Ui::formatTime(seconds) : QStringLiteral("—"));
    m_nameLabel->setText(hasNext && !name.isEmpty() ? name
                                                    : DayView::tr("No next prayer"));
    m_countdownLabel->setText(
        remaining >= 0
            ? DayView::tr("in %1").arg(Ui::formatCountdown(remaining))
            : QString());
    m_progress->setVisible(hasNext);
    const qreal progress = next.value(QStringLiteral("progress")).toDouble();
    m_progress->setValue(qRound((1.0 - progress) * 1000.0));
}

// ── PrayerTimesWidget ──────────────────────────────────────────────────────

PrayerTimesWidget::PrayerTimesWidget(QWidget *parent)
    : QGroupBox(DayView::tr("Prayer Times"), parent) {
    setObjectName(QStringLiteral("PrayerTimesGroup"));
    setAlignment(Qt::AlignLeft);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(0);

    m_table = new QTableWidget(6, 3, this);
    m_table->setObjectName(QStringLiteral("PrayerTable"));
    m_table->setAccessibleName(DayView::tr("Prayer times"));
    m_table->horizontalHeader()->hide();
    m_table->verticalHeader()->hide();
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setFrameShape(QFrame::StyledPanel);
    m_table->setFrameShadow(QFrame::Plain);
    m_table->setLineWidth(1);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    // All three columns are positioned explicitly so the "Raka'as" column
    // sits in the middle of the row instead of being pushed against the time.
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->viewport()->installEventFilter(this);
    Ui::compactTable(m_table, 18);
    // Exactly six rows plus the minimal frame around the table.
    m_table->setFixedHeight(
        m_table->verticalHeader()->defaultSectionSize() * 6
        + 2 * m_table->frameWidth());
    m_table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    applyTablePalette();
    updateColumnWidths();

    layout->addStretch(1);
    layout->addWidget(m_table);
    layout->addStretch(1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PrayerTimesWidget::applyTablePalette() {
    if (!m_table) {
        return;
    }
    const QPalette pal = palette();
    const QColor base = pal.color(QPalette::Window);
    QPalette tablePalette = m_table->palette();
    // Let the table blend into the card.  Breeze's Base is darker than the
    // window colour and its AlternateBase is darker still, which made the
    // zebra stripes look like holes in the card.
    tablePalette.setColor(QPalette::Base, base);
    tablePalette.setColor(QPalette::AlternateBase,
                          Ui::mixColor(base, pal.color(QPalette::WindowText),
                                       0.05));
    m_table->setPalette(tablePalette);
}

void PrayerTimesWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange) {
        applyTablePalette();
    }
    QGroupBox::changeEvent(event);
}

void PrayerTimesWidget::updateColumnWidths() {
    if (!m_table || !m_table->viewport()) {
        return;
    }
    const int available = m_table->viewport()->width();
    if (available <= 0) {
        return;
    }
    const QFont infoFont = Ui::scaledFont(font(), 0.9, false);
    const QFontMetrics metrics(infoFont);
    int infoWidth = 0;
    for (int row = 0; row < 6; ++row) {
        const QTableWidgetItem *item = m_table->item(row, 1);
        const QString text = item && !item->text().isEmpty()
                                 ? item->text()
                                 : DayView::tr(kPrayerInfo[row]);
        infoWidth = qMax(infoWidth, metrics.horizontalAdvance(text));
    }
    infoWidth += 18;
    const int side = qMax(0, (available - infoWidth) / 2);
    m_table->setColumnWidth(0, side);
    m_table->setColumnWidth(1, infoWidth);
    m_table->setColumnWidth(2, qMax(0, available - side - infoWidth));
}

bool PrayerTimesWidget::eventFilter(QObject *watched, QEvent *event) {
    if (m_table && watched == m_table->viewport()
        && event->type() == QEvent::Resize) {
        updateColumnWidths();
    }
    return QGroupBox::eventFilter(watched, event);
}

void PrayerTimesWidget::setState(const QJsonObject &state) {
    const QJsonArray times = state.value(QStringLiteral("times")).toArray();
    const int current = state.value(QStringLiteral("current"))
                            .toObject()
                            .value(QStringLiteral("index"))
                            .toInt(-1);
    const QFont base = font();
    const QFont nameFont = Ui::scaledFont(base, 1.0, false);
    const QFont infoFont = Ui::scaledFont(base, 0.9, false);
    const QFont timeFont = Ui::tabularFont(base, 1.0, false);
    const QColor tint = Ui::tintColor(palette().color(QPalette::Highlight), 44);

    for (int row = 0; row < 6 && row < times.size(); ++row) {
        const QJsonObject item = times.at(row).toObject();
        auto *nameItem = m_table->item(row, 0);
        auto *infoItem = m_table->item(row, 1);
        auto *timeItem = m_table->item(row, 2);
        if (!nameItem) {
            nameItem = makeItem(QString(), Qt::AlignLeft | Qt::AlignVCenter);
            m_table->setItem(row, 0, nameItem);
        }
        if (!infoItem) {
            infoItem = makeItem(QString(), Qt::AlignCenter);
            m_table->setItem(row, 1, infoItem);
        }
        if (!timeItem) {
            timeItem = makeItem(QString(), Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(row, 2, timeItem);
        }

        nameItem->setText(Ui::prayerName(row));
        infoItem->setText(DayView::tr(kPrayerInfo[row]));
        const int seconds = item.value(QStringLiteral("seconds")).toInt(-1);
        timeItem->setText(
            seconds >= 0
                ? Ui::formatTime(seconds)
                : item.value(QStringLiteral("short")).toString());

        const bool active = row == current;
        QFont name = nameFont;
        QFont info = infoFont;
        if (active) {
            name.setWeight(QFont::DemiBold);
            info.setWeight(QFont::DemiBold);
        }
        nameItem->setFont(name);
        infoItem->setFont(info);
        timeItem->setFont(timeFont);

        if (active) {
            nameItem->setBackground(tint);
            infoItem->setBackground(tint);
            timeItem->setBackground(tint);
            infoItem->setForeground(palette().color(QPalette::WindowText));
        } else {
            nameItem->setBackground(QBrush());
            infoItem->setBackground(QBrush());
            timeItem->setBackground(QBrush());
            infoItem->setForeground(palette().color(QPalette::PlaceholderText));
        }
    }
    updateColumnWidths();
}

// ── QiblahCard ─────────────────────────────────────────────────────────────

QiblahCard::QiblahCard(QWidget *parent)
    : QGroupBox(DayView::tr("Qiblah"), parent) {
    setObjectName(QStringLiteral("QiblahGroup"));
    setAlignment(Qt::AlignLeft);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::groupSpacing(), Ui::groupSpacing(),
                               Ui::groupSpacing(), Ui::groupSpacing());
    layout->setSpacing(Ui::groupSpacing());

    m_compass = new QiblahCompass(this);
    m_angle = new QLabel(this);
    m_angle->setObjectName(QStringLiteral("QiblahAngle"));
    m_angle->setFont(Ui::scaledFont(font(), 1.7, true));
    m_angle->setAlignment(Qt::AlignCenter);
    m_angle->setWordWrap(true);
    m_suffix = new QLabel(this);
    m_suffix->setObjectName(QStringLiteral("QiblahSuffix"));
    m_suffix->setFont(Ui::scaledFont(font(), 0.9, false));
    m_suffix->setForegroundRole(QPalette::PlaceholderText);
    m_suffix->setAlignment(Qt::AlignCenter);
    m_suffix->setWordWrap(true);

    layout->addWidget(m_compass, 1);
    layout->addWidget(m_angle);
    layout->addWidget(m_suffix);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void QiblahCard::setState(const QJsonObject &qibla) {
    const qreal bearing = qibla.value(QStringLiteral("bearing")).toDouble();
    m_compass->setBearing(bearing);
    const bool clockwise = qibla.value(QStringLiteral("clockwise")).toBool(true);
    if (clockwise) {
        m_angle->setText(QStringLiteral("%1°").arg(bearing, 0, 'f', 2));
        m_suffix->setText(DayView::tr("Clockwise from North"));
    } else {
        const QString cardinal =
            qibla.value(QStringLiteral("cardinal")).toString();
        m_angle->setText(cardinal.isEmpty()
                             ? QStringLiteral("%1°").arg(bearing, 0, 'f', 2)
                             : cardinal);
        m_suffix->setText(DayView::tr("from true North"));
    }
}

// ── ResponsiveRow ──────────────────────────────────────────────────────────

ResponsiveRow::ResponsiveRow(QWidget *first, QWidget *second, int breakpoint,
                             int spacing, int firstStretch, int secondStretch,
                             QWidget *parent, int horizontalMaxHeight)
    : QWidget(parent),
      m_breakpoint(breakpoint),
      m_firstStretch(firstStretch),
      m_secondStretch(secondStretch),
      m_horizontalMaxHeight(horizontalMaxHeight) {
    m_layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(spacing);
    m_layout->addWidget(first);
    m_layout->addWidget(second);
    m_layout->setStretch(0, m_firstStretch);
    m_layout->setStretch(1, m_secondStretch);
    setMaximumHeight(m_horizontalMaxHeight);
}

void ResponsiveRow::resizeEvent(QResizeEvent *event) {
    updateDirection();
    QWidget::resizeEvent(event);
}

void ResponsiveRow::updateDirection() {
    const bool stack = width() > 0 && width() < m_breakpoint;
    if (stack == m_stacked) {
        return;
    }
    m_stacked = stack;
    m_layout->setDirection(stack ? QBoxLayout::TopToBottom
                                 : QBoxLayout::LeftToRight);
    m_layout->setStretch(0, stack ? 0 : m_firstStretch);
    m_layout->setStretch(1, stack ? 0 : m_secondStretch);
    setMaximumHeight(stack ? QWIDGETSIZE_MAX : m_horizontalMaxHeight);
    m_layout->invalidate();
    updateGeometry();
}

// ── DayView ────────────────────────────────────────────────────────────────

DayView::DayView(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("DayPage"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("DayScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("DayContent"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::groupSpacing());
    layout->setSpacing(Ui::sectionSpacing());

    auto *header = new QWidget(page);
    header->setObjectName(QStringLiteral("DateHeader"));
    // Gregorian date on the left, Hijri date on the right: one full-width
    // line instead of two short labels with a large empty area beside them.
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(Ui::sectionSpacing());
    m_dateLabel = new QLabel(header);
    m_dateLabel->setObjectName(QStringLiteral("DateLabel"));
    m_dateLabel->setFont(Ui::scaledFont(font(), 1.2, true));
    m_dateLabel->setWordWrap(true);
    m_dateLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_hijriLabel = new QLabel(header);
    m_hijriLabel->setObjectName(QStringLiteral("HijriDateLabel"));
    m_hijriLabel->setFont(Ui::scaledFont(font(), 0.95, false));
    m_hijriLabel->setForegroundRole(QPalette::PlaceholderText);
    m_hijriLabel->setWordWrap(true);
    m_hijriLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_hijriLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    headerLayout->addWidget(m_dateLabel, 1);
    headerLayout->addWidget(m_hijriLabel, 0);

    m_timeCard = new CurrentTimeCard;
    m_nextCard = new NextPrayerCard;
    m_cards = new ResponsiveRow(m_timeCard, m_nextCard, 640,
                                Ui::controlSpacing() + 4, 1, 1, page);
    m_cards->setObjectName(QStringLiteral("CardsRow"));

    m_prayerTimes = new PrayerTimesWidget;
    m_qiblahCard = new QiblahCard;
    m_content = new ResponsiveRow(m_prayerTimes, m_qiblahCard, 780,
                                  Ui::sectionSpacing(), 3, 2, page, 340);
    m_content->setObjectName(QStringLiteral("ContentRow"));

    layout->addWidget(header);
    layout->addWidget(m_cards);
    layout->addWidget(m_content);
    layout->addStretch(1);

    scroll->setWidget(page);
    outer->addWidget(scroll);
}

void DayView::setState(const QJsonObject &state) {
    const QJsonObject date = state.value(QStringLiteral("date")).toObject();
    const QDate selected =
        QDate::fromString(date.value(QStringLiteral("iso")).toString(),
                          Qt::ISODate);
    m_dateLabel->setText(
        selected.isValid()
            ? QLocale().toString(selected, QLocale::LongFormat)
            : date.value(QStringLiteral("text")).toString());
    const QString hijri = Ui::formatHijriLine(date);
    m_hijriLabel->setText(hijri);
    m_hijriLabel->setVisible(!hijri.isEmpty());

    m_timeCard->setState(state);
    m_nextCard->setState(state);
    m_prayerTimes->setState(state);
    m_qiblahCard->setState(state.value(QStringLiteral("qibla")).toObject());
    m_qiblahCard->setVisible(
        state.value(QStringLiteral("showQiblah")).toBool(true));
}

// ── MonthView ──────────────────────────────────────────────────────────────

MonthView::MonthView(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("MonthPage"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(Ui::sectionSpacing());

    auto *calendars = new QHBoxLayout();
    calendars->setSpacing(Ui::sectionSpacing());

    auto *gregorianGroup = new QGroupBox(tr("Gregorian calendar"), this);
    gregorianGroup->setObjectName(QStringLiteral("GregorianGroup"));
    gregorianGroup->setAlignment(Qt::AlignLeft);
    m_gregorianGroup = gregorianGroup;
    auto *gregorianLayout = new QVBoxLayout(gregorianGroup);
    gregorianLayout->setContentsMargins(Ui::groupSpacing(), Ui::groupSpacing(),
                                        Ui::groupSpacing(), Ui::groupSpacing());
    m_calendar = new QCalendarWidget(gregorianGroup);
    m_calendar->setObjectName(QStringLiteral("GregorianCalendar"));
    m_calendar->setAccessibleName(tr("Gregorian calendar"));
    m_calendar->setGridVisible(false);
    m_calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    m_calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    m_calendar->setFirstDayOfWeek(Qt::Sunday);
    m_calendar->setNavigationBarVisible(true);
    m_calendar->setDateEditEnabled(false);
    gregorianLayout->addWidget(m_calendar);
    calendars->addWidget(gregorianGroup, 1);

    auto *hijriGroup = new QGroupBox(tr("Hijri calendar"), this);
    hijriGroup->setObjectName(QStringLiteral("HijriGroup"));
    hijriGroup->setAlignment(Qt::AlignLeft);
    m_hijriGroup = hijriGroup;
    auto *hijriLayout = new QVBoxLayout(hijriGroup);
    hijriLayout->setContentsMargins(Ui::groupSpacing(), Ui::groupSpacing(),
                                    Ui::groupSpacing(), Ui::groupSpacing());
    m_hijriTable = new QTableWidget(hijriGroup);
    m_hijriTable->setObjectName(QStringLiteral("HijriCalendar"));
    m_hijriTable->setAccessibleName(tr("Hijri calendar"));
    m_hijriTable->setColumnCount(7);
    QStringList dayNames;
    for (int day = 0; day < 7; ++day) {
        dayNames << QLocale().dayName(((day + 6) % 7) + 1,
                                      QLocale::ShortFormat);
    }
    m_hijriTable->setHorizontalHeaderLabels(dayNames);
    m_hijriTable->verticalHeader()->hide();
    m_hijriTable->setShowGrid(false);
    m_hijriTable->setFrameShape(QFrame::NoFrame);
    m_hijriTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_hijriTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_hijriTable->setFocusPolicy(Qt::NoFocus);
    for (int column = 0; column < 7; ++column) {
        m_hijriTable->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::Stretch);
    }
    Ui::compactTable(m_hijriTable, 8);
    // Let the Hijri month grid fill the same height as the Gregorian calendar
    // instead of leaving a large empty panel below the last week.
    m_hijriTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_hijriTable->setMinimumHeight(160);
    hijriLayout->addWidget(m_hijriTable);
    calendars->addWidget(hijriGroup, 1);

    layout->addLayout(calendars, 1);

    auto *timesGroup = new QGroupBox(tr("Prayer Times"), this);
    timesGroup->setObjectName(QStringLiteral("MonthPrayerTimesGroup"));
    timesGroup->setAlignment(Qt::AlignLeft);
    m_timesGroup = timesGroup;
    auto *timesLayout = new QVBoxLayout(timesGroup);
    timesLayout->setContentsMargins(Ui::groupSpacing(), Ui::groupSpacing(),
                                    Ui::groupSpacing(), Ui::groupSpacing());
    m_timesTable = new QTableWidget(1, 7, timesGroup);
    m_timesTable->setObjectName(QStringLiteral("MonthPrayerTable"));
    m_timesTable->setAccessibleName(tr("Prayer times for the selected date"));
    m_timesTable->setHorizontalHeaderLabels(
        {tr("Date"), tr("Fajr"), tr("Shuruq"), tr("Dhuhr"),
         tr("Asr"), tr("Maghrib"), tr("Ishaa")});
    m_timesTable->verticalHeader()->hide();
    m_timesTable->setShowGrid(false);
    m_timesTable->setFrameShape(QFrame::NoFrame);
    m_timesTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_timesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_timesTable->setFocusPolicy(Qt::NoFocus);
    m_timesTable->setAlternatingRowColors(false);
    for (int column = 0; column < 7; ++column) {
        m_timesTable->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::Stretch);
    }
    Ui::compactTable(m_timesTable, 8);
    m_timesTable->setFixedHeight(
        m_timesTable->horizontalHeader()->sizeHint().height()
        + m_timesTable->verticalHeader()->defaultSectionSize() + 4);
    timesLayout->addWidget(m_timesTable);
    layout->addWidget(timesGroup);

    connect(m_calendar, &QCalendarWidget::clicked, this, [this](const QDate &date) {
        if (!m_updating) {
            emit dateSelected(date);
        }
    });
    connect(m_calendar, &QCalendarWidget::currentPageChanged, this,
            [this](int year, int month) {
                if (m_updating) {
                    return;
                }
                const QDate page(year, month, 1);
                const int day = qMin(m_selectedDate.day(), page.daysInMonth());
                emit dateSelected(QDate(year, month, qMax(1, day)));
            });
    connect(m_hijriTable, &QTableWidget::cellClicked, this,
            [this](int row, int column) {
                if (m_updating) {
                    return;
                }
                if (auto *item = m_hijriTable->item(row, column)) {
                    const QDate date = QDate::fromString(
                        item->data(Qt::UserRole).toString(), Qt::ISODate);
                    if (date.isValid()) {
                        emit dateSelected(date);
                    }
                }
            });
}

void MonthView::setData(const QJsonObject &month, const QJsonObject &day) {
    m_updating = true;
    const QDate selected = QDate::fromString(
        day.value(QStringLiteral("date")).toObject()
            .value(QStringLiteral("iso")).toString(),
        Qt::ISODate);
    if (selected.isValid()) {
        m_selectedDate = selected;
    }

    const QString hijriTitle =
        QStringLiteral("%1 %2")
            .arg(Ui::hijriMonthName(month.value(QStringLiteral("hijriMonth")).toInt()),
                 QString::number(month.value(QStringLiteral("hijriYear")).toInt()));
    const QString selectedLabel = selected.isValid()
                                      ? QLocale().toString(selected, QStringLiteral("d MMM"))
                                      : month.value(QStringLiteral("selectedLabel")).toString();

    if (auto *group = qobject_cast<QGroupBox *>(m_hijriGroup)) {
        group->setTitle(hijriTitle);
    }
    if (auto *group = qobject_cast<QGroupBox *>(m_timesGroup)) {
        group->setTitle(tr("Prayer Times — %1").arg(selectedLabel));
    }

    rebuildHijri(month.value(QStringLiteral("cellsH")).toArray(), hijriTitle);
    populateTimes(day, selectedLabel);

    if (m_selectedDate.isValid()) {
        m_calendar->setSelectedDate(m_selectedDate);
    }
    m_updating = false;
}

void MonthView::rebuildHijri(const QJsonArray &cells, const QString &title) {
    Q_UNUSED(title);
    m_hijriTable->clearContents();
    const int rows = qMax(4, (cells.size() + 6) / 7);
    m_hijriTable->setRowCount(rows);
    m_hijriTable->setColumnCount(7);

    const QFont base = font();
    const QFont normal = Ui::scaledFont(base, 0.95, false);
    const QFont emphasized = Ui::scaledFont(base, 0.95, true);
    const QColor tint = Ui::tintColor(palette().color(QPalette::Highlight));

    for (int index = 0; index < rows * 7; ++index) {
        const int row = index / 7;
        const int column = index % 7;
        auto *item = new QTableWidgetItem;
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(Qt::ItemIsEnabled);
        if (index < cells.size()) {
            const QJsonObject cell = cells.at(index).toObject();
            const int day = cell.value(QStringLiteral("day")).toInt();
            if (day > 0) {
                item->setText(QString::number(day));
            }
            item->setData(Qt::UserRole,
                          cell.value(QStringLiteral("iso")).toString());
            const bool selectedCell = cell.value(QStringLiteral("selected")).toBool();
            const bool todayCell = cell.value(QStringLiteral("today")).toBool();
            item->setFont(selectedCell || todayCell ? emphasized : normal);
            if (selectedCell) {
                item->setBackground(tint);
            }
            if (!cell.value(QStringLiteral("inMonth")).toBool(true)) {
                item->setForeground(palette().color(QPalette::PlaceholderText));
            }
        }
        m_hijriTable->setItem(row, column, item);
    }
}

void MonthView::populateTimes(const QJsonObject &day, const QString &selectedLabel) {
    auto *dateItem = m_timesTable->item(0, 0);
    if (!dateItem) {
        dateItem = makeItem(QString(), Qt::AlignCenter);
        m_timesTable->setItem(0, 0, dateItem);
    }
    dateItem->setText(selectedLabel);

    const QJsonArray times = day.value(QStringLiteral("times")).toArray();
    const QFont timeFont = Ui::tabularFont(m_timesTable->font(), 1.0, false);
    for (int index = 0; index < 6; ++index) {
        auto *item = m_timesTable->item(0, index + 1);
        if (!item) {
            item = makeItem(QString(), Qt::AlignCenter);
            m_timesTable->setItem(0, index + 1, item);
        }
        const QJsonObject prayer = index < times.size()
                                       ? times.at(index).toObject()
                                       : QJsonObject();
        const int seconds = prayer.value(QStringLiteral("seconds")).toInt(-1);
        item->setText(seconds >= 0
                          ? Ui::formatTime(seconds)
                          : prayer.value(QStringLiteral("short")).toString());
        item->setFont(timeFont);
    }
}

// ── YearView ───────────────────────────────────────────────────────────────

YearView::YearView(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("YearPage"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::pageMargin(), Ui::pageMargin(),
                               Ui::pageMargin(), Ui::pageMargin());
    layout->setSpacing(Ui::sectionSpacing());

    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("YearTitle"));
    m_title->setFont(Ui::scaledFont(font(), 1.1, true));
    m_title->setWordWrap(true);
    m_title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_title);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("YearTable"));
    m_table->setAccessibleName(tr("Prayer times for the next year"));
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels(
        {tr("Date"), tr("Fajr"), tr("Shuruq"), tr("Dhuhr"),
         tr("Asr"), tr("Maghrib"), tr("Ishaa")});
    m_table->verticalHeader()->hide();
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    for (int column = 0; column < 7; ++column) {
        m_table->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::Stretch);
    }
    Ui::compactTable(m_table, 10);
    layout->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (auto *item = m_table->item(row, 0)) {
            const QDate date = QDate::fromString(
                item->data(Qt::UserRole).toString(), Qt::ISODate);
            if (date.isValid()) {
                emit openDayRequested(date);
            }
        }
    });
    connect(m_table, &QTableWidget::itemActivated, this, [this](QTableWidgetItem *item) {
        if (!item) {
            return;
        }
        auto *dateItem = m_table->item(item->row(), 0);
        if (!dateItem) {
            return;
        }
        const QDate date = QDate::fromString(
            dateItem->data(Qt::UserRole).toString(), Qt::ISODate);
        if (date.isValid()) {
            emit openDayRequested(date);
        }
    });
}

void YearView::setData(const QJsonObject &year) {
    const QDate start = QDate::fromString(
        year.value(QStringLiteral("startIso")).toString(), Qt::ISODate);
    const QDate end = QDate::fromString(
        year.value(QStringLiteral("endIso")).toString(), Qt::ISODate);
    if (start.isValid() && end.isValid()) {
        m_title->setText(QStringLiteral("%1 — %2")
                             .arg(QLocale().toString(start, QLocale::LongFormat),
                                  QLocale().toString(end, QLocale::LongFormat)));
    } else {
        m_title->setText(year.value(QStringLiteral("title")).toString());
    }
    const QJsonArray rows = year.value(QStringLiteral("rows")).toArray();
    const QFont base = font();
    const QFont normal = Ui::scaledFont(base, 0.95, false);
    const QFont emphasized = Ui::scaledFont(base, 0.95, true);
    const QFont timeFont = Ui::tabularFont(base, 0.95, false);
    const QColor tint = Ui::tintColor(palette().color(QPalette::Highlight));

    m_table->setUpdatesEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(rows.size());
    const QString keys[] = {QStringLiteral("date"), QStringLiteral("fajr"),
                            QStringLiteral("shuruq"), QStringLiteral("dhuhr"),
                            QStringLiteral("asr"), QStringLiteral("maghrib"),
                            QStringLiteral("ishaa")};
    for (int row = 0; row < rows.size(); ++row) {
        const QJsonObject object = rows.at(row).toObject();
        const bool current = object.value(QStringLiteral("current")).toBool();
        for (int column = 0; column < 7; ++column) {
            QString text = object.value(keys[column]).toString();
            if (column == 0) {
                const QDate date = QDate::fromString(
                    object.value(QStringLiteral("iso")).toString(), Qt::ISODate);
                if (date.isValid()) {
                    text = QLocale().toString(date, QStringLiteral("d MMM"));
                }
            }
            auto *item = makeItem(
                text,
                column == 0 ? (Qt::AlignLeft | Qt::AlignVCenter)
                            : Qt::AlignCenter);
            if (column == 0) {
                item->setData(Qt::UserRole,
                              object.value(QStringLiteral("iso")).toString());
            }
            item->setFont(column == 0 ? (current ? emphasized : normal) : timeFont);
            if (current) {
                item->setBackground(tint);
            }
            m_table->setItem(row, column, item);
        }
    }
    m_table->setUpdatesEnabled(true);
    if (m_table->rowCount() > 0) {
        m_table->selectRow(0);
        m_table->scrollToTop();
    }
}

// ── MasjidWindow ───────────────────────────────────────────────────────────

MasjidWindow::MasjidWindow(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Window);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Ui::blockSpacing() * 2, Ui::blockSpacing() * 2,
                               Ui::blockSpacing() * 2, Ui::blockSpacing() * 2);
    layout->setSpacing(Ui::sectionSpacing());

    m_date = new QLabel(this);
    m_date->setAlignment(Qt::AlignCenter);
    m_date->setFont(Ui::scaledFont(font(), 1.4, false));
    Ui::styleCaption(m_date);

    m_clock = new QLabel(QStringLiteral("--:--:--"), this);
    m_clock->setAlignment(Qt::AlignCenter);
    m_clock->setFont(Ui::tabularFont(font(), 5.2, true));

    m_next = new QLabel(this);
    m_next->setAlignment(Qt::AlignCenter);
    m_next->setFont(Ui::scaledFont(font(), 1.6, true));

    m_iqama = new QLabel(this);
    m_iqama->setAlignment(Qt::AlignCenter);
    m_iqama->setFont(Ui::scaledFont(font(), 1.3, true));
    m_iqama->hide();

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 1000);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(8);

    layout->addWidget(m_date);
    layout->addWidget(m_clock, 1);
    layout->addWidget(m_next);
    layout->addWidget(m_iqama);
    layout->addWidget(m_progress);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    auto *times = new QHBoxLayout();
    times->setSpacing(Ui::sectionSpacing());
    const QString names[] = {tr("Fajr"), tr("Shuruq"), tr("Dhuhr"),
                             tr("Asr"), tr("Maghrib"), tr("Ishaa")};
    for (int index = 0; index < 6; ++index) {
        auto *column = new QWidget(this);
        auto *columnLayout = new QVBoxLayout(column);
        columnLayout->setContentsMargins(0, 0, 0, 0);
        columnLayout->setSpacing(Ui::spacingTight());
        m_names[index] = new QLabel(names[index], column);
        m_names[index]->setAlignment(Qt::AlignCenter);
        Ui::styleCaption(m_names[index]);
        m_times[index] = new QLabel(QStringLiteral("--:--"), column);
        m_times[index]->setAlignment(Qt::AlignCenter);
        m_times[index]->setFont(Ui::tabularFont(font(), 1.5, true));
        columnLayout->addWidget(m_names[index]);
        columnLayout->addWidget(m_times[index]);
        times->addWidget(column, 1);
    }
    layout->addLayout(times);
}

void MasjidWindow::setState(const QJsonObject &state) {
    const QJsonObject now = state.value(QStringLiteral("now")).toObject();
    const QJsonObject date = state.value(QStringLiteral("date")).toObject();
    const QJsonObject next = state.value(QStringLiteral("next")).toObject();
    const int nowSeconds = now.value(QStringLiteral("seconds")).toInt(-1);
    m_clock->setText(nowSeconds >= 0
                         ? Ui::formatTime(nowSeconds, true)
                         : now.value(QStringLiteral("text")).toString());
    m_date->setText(Ui::formatDateLine(date));

    const QString nextName = Ui::prayerName(
        next.value(QStringLiteral("index")).toInt(-1));
    const int nextSeconds = next.value(QStringLiteral("seconds")).toInt(-1);
    const int remaining = next.value(QStringLiteral("remaining")).toInt(-1);
    if (nextSeconds >= 0 && !nextName.isEmpty()) {
        m_next->setText(tr("Next: %1 at %2 — %3")
                            .arg(nextName, Ui::formatTime(nextSeconds),
                                 Ui::formatCountdown(remaining)));
    } else if (nextName.isEmpty()) {
        m_next->setText(QStringLiteral("—"));
    } else {
        m_next->setText(tr("Next: %1").arg(nextName));
    }

    const qreal progress = next.value(QStringLiteral("progress")).toDouble();
    m_progress->setVisible(nextSeconds >= 0);
    m_progress->setValue(qRound((1.0 - progress) * 1000.0));

    const QJsonArray times = state.value(QStringLiteral("times")).toArray();
    for (int index = 0; index < 6; ++index) {
        const QJsonObject prayer = index < times.size()
                                       ? times.at(index).toObject()
                                       : QJsonObject();
        const int seconds = prayer.value(QStringLiteral("seconds")).toInt(-1);
        m_times[index]->setText(seconds >= 0
                                    ? Ui::formatTime(seconds)
                                    : QStringLiteral("—"));
    }

    const QJsonArray iqama = state.value(QStringLiteral("iqama")).toArray();
    const QJsonArray iqamaOn = state.value(QStringLiteral("iqamaOn")).toArray();
    const int current = state.value(QStringLiteral("current"))
                            .toObject()
                            .value(QStringLiteral("index"))
                            .toInt(-1);
    bool showIqama = false;
    if (current >= 0 && current < 6 && current < iqama.size()
        && current < iqamaOn.size() && iqamaOn.at(current).toBool()
        && iqama.at(current).toInt() > 0) {
        const int prayerSeconds = times.size() > current
                                      ? times.at(current).toObject()
                                            .value(QStringLiteral("seconds"))
                                            .toInt(-1)
                                      : -1;
        const int delay = iqama.at(current).toInt() * 60;
        const int nowSeconds2 = now.value(QStringLiteral("seconds")).toInt(-1);
        if (prayerSeconds >= 0 && nowSeconds2 >= 0) {
            int left = (prayerSeconds + delay - nowSeconds2) % (24 * 3600);
            if (left < 0) {
                left += 24 * 3600;
            }
            if (left > 0 && left <= 6 * 3600) {
                m_iqama->setText(tr("Iqama in %1").arg(Ui::formatCountdown(left)));
                showIqama = true;
            }
        }
    }
    m_iqama->setVisible(showIqama);
}

void MasjidWindow::setScreen(QScreen *screen) {
    if (!screen) {
        return;
    }
    if (!windowHandle()) {
        createWinId();
    }
    if (auto *handle = windowHandle()) {
        handle->setScreen(screen);
    }
    setGeometry(screen->geometry());
}

void MasjidWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

void MasjidWindow::mousePressEvent(QMouseEvent *) {
    hide();
}
