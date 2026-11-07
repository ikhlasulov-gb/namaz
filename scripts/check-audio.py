#!/usr/bin/env python3
"""Manual audio smoke test against the real PipeWire/PulseAudio session.

Requires a running graphical session with an audio server.  It loads the
debug core library, plays every supported source type, verifies that a sink
input is active, then stops it.

Usage: check-audio.py [path/to/libnamaz_core.so]
"""
from __future__ import annotations

import ctypes
import json
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_LIB = os.path.join(ROOT, "ui/build/cargo/debug/libnamaz_core.so")
WAVE = os.path.join(ROOT, "assets/audio")


def main() -> int:
    lib_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_LIB
    lib = ctypes.CDLL(lib_path)
    lib.namaz_core_play_sound.argtypes = [ctypes.c_char_p]
    lib.namaz_core_play_path.argtypes = [ctypes.c_char_p]
    lib.namaz_core_play_folder.argtypes = [ctypes.c_char_p]
    lib.namaz_core_sound_exists.argtypes = [ctypes.c_char_p]
    for name in (
        "namaz_core_play_sound",
        "namaz_core_play_path",
        "namaz_core_play_folder",
        "namaz_core_play_quran_random",
        "namaz_core_play_random_athan",
        "namaz_core_play_iqama_random",
        "namaz_core_sound_exists",
        "namaz_core_is_playing",
    ):
        getattr(lib, name).restype = ctypes.c_int
    lib.namaz_core_stop_audio.restype = None

    failures = 0

    def check(condition: bool, message: str) -> None:
        nonlocal failures
        print(("PASS " if condition else "FAIL ") + message)
        if not condition:
            failures += 1

    def playing() -> bool:
        return lib.namaz_core_is_playing() != 0

    def stop() -> None:
        lib.namaz_core_stop_audio()
        time.sleep(0.4)

    check(lib.namaz_core_sound_exists(b"ADHAN") == 1,
          "ADHAN.ogg is available")
    check(lib.namaz_core_sound_exists(b"BISMILLAH") == 1,
          "BISMILLAH.ogg is available")
    check(lib.namaz_core_sound_exists(b"ALIHLAS") == 1,
          "ALIHLAS.ogg is available")
    check(lib.namaz_core_sound_exists(b"SHURUQ") == 0,
          "missing SHURUQ.wav is reported as missing")
    check(lib.namaz_core_sound_exists(b"DUA") == 0,
          "optional DUA is reported as missing")
    check(lib.namaz_core_sound_exists(b"IQAMA0") == 0,
          "optional IQAMA is reported as missing")

    checks = [
        ("play_sound", lambda: lib.namaz_core_play_sound(b"ADHAN")),
        ("play_path", lambda: lib.namaz_core_play_path(
            os.path.join(WAVE, "ADHAN.ogg").encode())),
        ("play_folder", lambda: lib.namaz_core_play_folder(WAVE.encode())),
        ("play_quran_random", lib.namaz_core_play_quran_random),
        ("play_random_athan", lib.namaz_core_play_random_athan),
    ]
    if lib.namaz_core_sound_exists(b"IQAMA0") == 1:
        checks.append(("play_iqama_random", lib.namaz_core_play_iqama_random))
    for name, start in checks:
        result = start()
        time.sleep(1.0)
        check(result != 0 and playing(), f"{name} starts a sink")
        stop()
        check(not playing(), f"{name} stops cleanly")

    print(f"{failures} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
