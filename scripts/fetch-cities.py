#!/usr/bin/env python3
"""Build the bundled city database from GeoNames.

GeoNames publishes its data under the Creative Commons Attribution 4.0
licence.  The generated file keeps the small schema used by the Rust core:
country name, city name, state/region, latitude, longitude and the standard
UTC offset in hours plus a daylight-saving flag.

Usage:
    scripts/fetch-cities.py
    scripts/fetch-cities.py --cache /tmp/geonames --output assets/cities.json
"""
from __future__ import annotations

import argparse
import io
import json
import sys
import urllib.request
import zipfile
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

BASE = "https://download.geonames.org/export/dump"
CITIES_URL = f"{BASE}/cities5000.zip"
COUNTRIES_URL = f"{BASE}/countryInfo.txt"
ADMIN1_URL = f"{BASE}/admin1CodesASCII.txt"
SOURCE = "GeoNames cities5000 (CC BY 4.0)"


def download(url: str, destination: Path) -> Path:
    if destination.exists() and destination.stat().st_size > 0:
        return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    print(f"downloading {url}", file=sys.stderr)
    with urllib.request.urlopen(url, timeout=120) as response:
        destination.write_bytes(response.read())
    return destination


def read_countries(path: Path) -> dict[str, str]:
    names: dict[str, str] = {}
    with path.open(encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("#") or not line.strip():
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) >= 5:
                names[fields[0]] = fields[4]
    return names


def read_admin1(path: Path) -> dict[str, str]:
    names: dict[str, str] = {}
    with path.open(encoding="utf-8") as handle:
        for line in handle:
            fields = line.rstrip("\n").split("\t")
            if len(fields) >= 2:
                names[fields[0]] = fields[1]
    return names


def utc_offset_and_dst(zone_name: str) -> tuple[float, bool]:
    if not zone_name:
        return 0.0, False
    try:
        zone = ZoneInfo(zone_name)
    except Exception:
        return 0.0, False
    winter = datetime(2026, 1, 15, 12, tzinfo=zone).utcoffset()
    summer = datetime(2026, 7, 15, 12, tzinfo=zone).utcoffset()
    if winter is None or summer is None:
        return 0.0, False
    winter_hours = winter.total_seconds() / 3600.0
    summer_hours = summer.total_seconds() / 3600.0
    return min(winter_hours, summer_hours), winter_hours != summer_hours


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=Path("/tmp/namaz-geonames"))
    parser.add_argument("--output", type=Path, default=Path("assets/cities.json"))
    args = parser.parse_args()

    cache = args.cache
    cities_zip = download(CITIES_URL, cache / "cities5000.zip")
    countries_txt = download(COUNTRIES_URL, cache / "countryInfo.txt")
    admin1_txt = download(ADMIN1_URL, cache / "admin1CodesASCII.txt")

    country_names = read_countries(countries_txt)
    admin1_names = read_admin1(admin1_txt)

    countries: dict[str, list[dict]] = {}
    seen: set[tuple] = set()
    with zipfile.ZipFile(cities_zip) as archive:
        with archive.open("cities5000.txt") as raw:
            for line in io.TextIOWrapper(raw, encoding="utf-8"):
                fields = line.rstrip("\n").split("\t")
                if len(fields) < 18:
                    continue
                name = fields[1]
                country_code = fields[8]
                admin1_code = fields[10]
                if not name or not country_code:
                    continue
                lat = float(fields[4])
                lon = float(fields[5])
                state = admin1_names.get(f"{country_code}.{admin1_code}", "")
                key = (name, country_code, state, round(lat, 4), round(lon, 4))
                if key in seen:
                    continue
                seen.add(key)
                tz, dst = utc_offset_and_dst(fields[17])
                country = country_names.get(country_code, country_code)
                countries.setdefault(country, []).append(
                    {
                        "name": name,
                        "state": state,
                        "lat": round(lat, 4),
                        "lon": round(lon, 4),
                        "tz": tz,
                        "dst": dst,
                    }
                )

    result = {
        "source": SOURCE,
        "license": "CC BY 4.0",
        "generated": datetime.now().date().isoformat(),
        "countries": [
            {"name": country, "cities": sorted(cities, key=lambda item: item["name"])}
            for country, cities in sorted(countries.items())
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )
    total = sum(len(item["cities"]) for item in result["countries"])
    print(
        f"{len(result['countries'])} countries, {total} cities -> "
        f"{args.output} ({args.output.stat().st_size / 1024 / 1024:.1f} MiB)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
