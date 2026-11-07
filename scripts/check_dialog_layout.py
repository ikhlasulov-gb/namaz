#!/usr/bin/env python3
"""Check compact layout invariants of the settings dialogs."""

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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--actual", type=Path, required=True)
    args = parser.parse_args()

    payload = json.loads(args.actual.read_text())
    widgets: dict[str, dict] = {}
    flatten(payload["root"], widgets)

    failures = 0

    def check(condition: bool, message: str) -> None:
        nonlocal failures
        print(("OK   " if condition else "FAIL ") + message)
        if not condition:
            failures += 1

    if "VolumeSlider" in widgets:
        slider = widgets["VolumeSlider"]
        check(slider["w"] <= 240,
              f"volume slider is compact: {slider['w']}px")
    check("PreferencesNav" in widgets or "PrayerSettingsNav" in widgets,
          "settings use a sidebar navigation list")
    check("PreferencesStack" in widgets or "PrayerSettingsStack" in widgets,
          "settings use a stacked content area")
    for name in ("PreferencesStack", "PrayerSettingsStack"):
        if name in widgets:
            check(widgets[name]["w"] > 0 and widgets[name]["h"] > 0,
                  f"{name} is laid out")

    if failures:
        print(f"\n{failures} dialog layout check(s) failed", file=sys.stderr)
        return 1
    print("\ndialog layout invariants hold")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
