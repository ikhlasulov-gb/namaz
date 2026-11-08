#!/usr/bin/env python3
"""Check the native KDE/HIG layout invariants of the Widgets frontend."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def flatten(node: dict, out: dict[str, dict]) -> None:
    name = node.get("name", "")
    if name:
        out[name] = node
    for child in node.get("children", []):
        flatten(child, out)


def rect(entry: dict) -> tuple[int, int, int, int]:
    return (entry["x"], entry["y"], entry["w"], entry["h"])


class Checker:
    def __init__(self):
        self.failures = 0

    def check(self, condition: bool, message: str):
        print(("OK   " if condition else "FAIL ") + message)
        if not condition:
            self.failures += 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--actual", type=Path,
                        default=Path("/tmp/namaz-ui-geometry.json"))
    parser.add_argument("--narrow", action="store_true",
                        help="Check the stacked layout of a narrow window")
    args = parser.parse_args()

    payload = json.loads(args.actual.read_text())
    widgets: dict[str, dict] = {}
    flatten(payload["root"], widgets)

    c = Checker()
    required = [
        "MainToolBar", "ViewStack", "DayPage", "DayScroll", "DayContent",
        "DateHeader", "DateLabel", "HijriDateLabel", "CardsRow",
        "CurrentTimeCard", "NextPrayerCard", "ContentRow", "PrayerTimesGroup",
        "PrayerTable", "QiblahGroup", "QiblahCompass", "QiblahAngle",
        "MonthPage", "GregorianCalendar", "HijriCalendar", "MonthPrayerTable",
        "YearPage", "YearTable",
    ]
    for name in required:
        c.check(name in widgets, f"{name} exists")
    if c.failures:
        return 1

    legacy = ["NowGroup", "QiblahArrow", "HeroCard", "PrayerTimesCard",
              "QiblahCard", "CalendarPane", "PrayerStrip"]
    for name in legacy:
        c.check(name not in widgets, f"legacy widget removed: {name}")

    c.check(widgets["ViewStack"].get("class") == "QStackedWidget",
            "views use QStackedWidget")
    toolbar = widgets["MainToolBar"]
    c.check(30 <= toolbar["h"] <= 52,
            f"toolbar follows the native height: {toolbar['h']}px")

    day = widgets["DayPage"]
    date = widgets["DateLabel"]
    hijri = widgets["HijriDateLabel"]
    cards = widgets["CardsRow"]
    current = widgets["CurrentTimeCard"]
    next_card = widgets["NextPrayerCard"]
    content = widgets["ContentRow"]
    table_group = widgets["PrayerTimesGroup"]
    table = widgets["PrayerTable"]
    qiblah = widgets["QiblahGroup"]
    compass = widgets["QiblahCompass"]
    angle = widgets["QiblahAngle"]

    dx, dy, dw, dh = rect(day)
    header = widgets["DateHeader"]
    c.check(date["y"] >= dx - 1 and date["y"] < cards["y"],
            "date header is above the cards")
    c.check(abs(hijri["y"] - date["y"]) <= 2 and hijri["y"] < cards["y"],
            "Hijri date shares the Gregorian date line")
    c.check(abs((hijri["x"] + hijri["w"]) - (header["x"] + header["w"])) <= 2,
            "Hijri date is right-aligned in the header")
    c.check(cards["y"] > date["y"] and content["y"] > cards["y"],
            "cards and content are ordered top to bottom")

    # Two independent cards: same row, same width and height when wide enough.
    cx, cy, cw, ch = rect(current)
    nx, ny, nw, nh = rect(next_card)

    # Prayer times and Qiblah.
    tx, ty, tw, th = rect(table_group)
    qx, qy, qw, qh = rect(qiblah)

    if not args.narrow:
        c.check(abs(cy - ny) <= 2, "time and next-prayer cards share a row")
        c.check(abs(cw - nw) <= 2, f"cards have equal width: {cw} vs {nw}")
        c.check(abs(ch - nh) <= 2, f"cards have equal height: {ch} vs {nh}")
        c.check(nx >= cx + cw, "next-prayer card sits beside the time card")
        current_prayer = widgets["CurrentPrayerLabel"]
        countdown = widgets["NextPrayerCountdown"]
        c.check(current_prayer["x"] > cx + cw / 2,
                "current prayer is aligned to the card's right edge")
        c.check(countdown["x"] > nx + nw / 2,
                "countdown is aligned to the card's right edge")
        c.check(abs(ty - qy) <= 2, "prayer times and Qiblah share a row")
        c.check(abs(th - qh) <= 2,
                f"sections have equal height: {th} vs {qh}")
        c.check(qx >= tx + tw, "Qiblah sits beside the prayer table")
        if qw:
            ratio = tw / qw
            c.check(1.3 <= ratio <= 1.7,
                    f"prayer/Qiblah ratio is near 3:2: {ratio:.2f}")
        c.check(abs(widgets["DayContent"]["w"]
                    - widgets["DayScroll"]["w"]) <= 1,
                "default window has no vertical scrollbar")
    else:
        c.check(current["y"] < next_card["y"]
                and next_card["y"] >= current["y"] + current["h"],
                "narrow: cards stack vertically")
        c.check(qiblah["y"] >= table_group["y"] + table_group["h"],
                "narrow: Qiblah is below the prayer table")
        c.check(abs(qiblah["x"] - table_group["x"]) <= 2,
                "narrow: both sections start on the same grid line")
        c.check(widgets["DayContent"]["w"]
                    >= widgets["DayScroll"]["w"] - 20,
                "narrow: no horizontal scrollbar")

    c.check(abs((cx - dx) - (dx + dw - (qx + qw))) <= 3,
            "day page side margins are symmetric")

    c.check(table.get("rows") == 6, f"prayer table rows: {table.get('rows')}")
    c.check(table["h"] % 6 == 0,
            f"prayer table has no extra bottom strip: {table['h']}px")
    c.check(table.get("columns") == 3,
            f"prayer table columns: {table.get('columns')}")
    widths = table.get("columnWidths", [])
    c.check(len(widths) == 3, "prayer table exposes three column widths")
    if len(widths) == 3:
        c.check(widths[0] > widths[1] and widths[0] > widths[2],
                f"prayer name column gets the free width: {widths}")
        c.check(widths[1] > 0 and widths[2] > 0,
                f"info and time columns keep their content width: {widths}")
    row_height = table["h"] / 6
    # Qt/font metrics differ between distributions: 32.3 px on Ubuntu 24.04,
    # 35.3 px on AlmaLinux 10.  Keep the row in the HIG ballpark without
    # making CI depend on a single font version.
    c.check(30 <= row_height <= 42,
            f"prayer rows follow the HIG height: {row_height:.1f}px")
    c.check(tx < table["x"] and ty < table["y"]
            and table["x"] + table["w"] < tx + tw
            and table["y"] + table["h"] < ty + th,
            "prayer table is inside its group box")

    # The compass must be a real, comfortably sized widget inside the card.
    c.check(compass["w"] >= 150 and compass["h"] >= 150,
            f"Qiblah compass is large enough: {compass['w']}x{compass['h']}")
    c.check(qx <= compass["x"] and compass["x"] + compass["w"] <= qx + qw
            and qy <= compass["y"] and compass["y"] + compass["h"] <= qy + qh,
            "Qiblah compass is inside its card")
    c.check(angle["y"] >= compass["y"] + compass["h"],
            "Qiblah angle is below the compass")

    if not args.narrow:
        month_times = widgets["MonthPrayerTable"]
        c.check(month_times.get("rows") == 1,
                f"month prayer table rows: {month_times.get('rows')}")
        c.check(month_times.get("columns") == 7,
                f"month prayer table columns: {month_times.get('columns')}")
        year = widgets["YearTable"]
        c.check(year.get("columns") == 7,
                f"year table columns: {year.get('columns')}")

    if c.failures:
        print(f"\n{c.failures} layout check(s) failed", file=sys.stderr)
        return 1
    print("\nnative HIG layout invariants hold")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
