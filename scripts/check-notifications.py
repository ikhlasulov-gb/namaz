#!/usr/bin/env python3
"""Manual desktop-notification smoke test on a real session.

Configures a fixed reminder for the current minute, calls Core::tick_events
and checks that org.freedesktop.Notifications.Notify was actually emitted.

Requires dbus-monitor, a running D-Bus session and a notification service.
"""
from __future__ import annotations

import ctypes
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_LIB = os.path.join(ROOT, "ui/build/cargo/debug/libnamaz_core.so")


def main() -> int:
    lib_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_LIB
    home = tempfile.mkdtemp(prefix="namaz-notify-")
    os.environ["HOME"] = home

    lib = ctypes.CDLL(lib_path)
    lib.namaz_core_new.restype = ctypes.c_void_p
    lib.namaz_core_day_json.argtypes = [ctypes.c_void_p]
    lib.namaz_core_day_json.restype = ctypes.c_void_p
    lib.namaz_core_tick_events.argtypes = [ctypes.c_void_p]
    lib.namaz_core_tick_events.restype = ctypes.c_void_p
    lib.namaz_core_apply_preferences_json.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.namaz_core_free_string.argtypes = [ctypes.c_void_p]
    lib.namaz_core_free.argtypes = [ctypes.c_void_p]

    def take(fn, core: int) -> str:
        pointer = fn(core)
        text = ctypes.string_at(pointer).decode()
        lib.namaz_core_free_string(pointer)
        return text

    monitor = subprocess.Popen(
        ["dbus-monitor", "--session", "interface='org.freedesktop.Notifications'"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    time.sleep(0.5)
    core = lib.namaz_core_new()
    day = json.loads(take(lib.namaz_core_day_json, core))
    minute = day["now"]["seconds"] // 60
    prefs = {
        "notifications_enabled": True,
        "reminder_visual_notification": True,
        "athan_enabled": False,
        "reminder_audio_mode": 0,
        "reminders_before_fixed": [minute, 0, 0, 0, 0, 0],
        "reminders_fixed_on": [True, False, False, False, False, False],
    }
    applied = lib.namaz_core_apply_preferences_json(
        core, json.dumps(prefs).encode()
    )
    events = take(lib.namaz_core_tick_events, core)
    lib.namaz_core_free(core)
    time.sleep(1.0)
    monitor.terminate()
    output = monitor.communicate(timeout=5)[0]

    failures = 0

    def check(condition: bool, message: str) -> None:
        nonlocal failures
        print(("PASS " if condition else "FAIL ") + message)
        if not condition:
            failures += 1

    check(applied == 1, "preferences applied")
    check('"type":"reminder"' in events.replace(" ", ""),
          "tick produced a reminder event")
    check("member=Notify" in output, "Notify reached the notification service")
    check("Namaz Reminder" in output, "notification title present")
    print(f"{failures} failure(s)")
    shutil.rmtree(home, ignore_errors=True)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
