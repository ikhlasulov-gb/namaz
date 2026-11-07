//! Bundled city database generated from GeoNames `cities5000` (CC BY 4.0).

use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};

type TzCache = Mutex<HashMap<(i64, i64), Option<String>>>;

use serde::Deserialize;
use tzf_rs::DefaultFinder;

#[derive(Debug, Clone, Deserialize)]
struct File {
    countries: Vec<Country>,
}

#[derive(Debug, Clone, Deserialize)]
struct Country {
    name: String,
    cities: Vec<RawCity>,
}

#[derive(Debug, Clone, Deserialize)]
struct RawCity {
    name: String,
    state: String,
    lat: f64,
    lon: f64,
    tz: f64,
}

#[derive(Debug, Clone)]
pub struct City {
    pub country: String,
    pub name: String,
    pub state: String,
    pub lat: f64,
    pub lon: f64,
    pub tz: f64,
}

pub fn load() -> Vec<City> {
    let text = include_str!("../../../assets/cities.json");
    let file: File = serde_json::from_str(text).expect("bundled cities.json is valid");
    let mut out = Vec::with_capacity(10_000);
    for country in file.countries {
        for city in country.cities {
            out.push(City {
                country: country.name.clone(),
                name: city.name,
                state: city.state,
                lat: city.lat,
                lon: city.lon,
                tz: city.tz,
            });
        }
    }
    out
}

/// Resolve an IANA time zone for a coordinate.
///
/// The bundled city database only stores a numeric standard-time offset and a
/// "uses daylight saving" flag, which is not enough to calculate DST rules.
/// tzf-rs maps the coordinates to a proper IANA zone so the frontend can use
/// the automatic DST mode without hard-coded country tables.
pub fn timezone_name(lon: f64, lat: f64) -> Option<String> {
    static FINDER: OnceLock<DefaultFinder> = OnceLock::new();
    static CACHE: OnceLock<TzCache> = OnceLock::new();

    let key = (
        (lon * 10_000.0).round() as i64,
        (lat * 10_000.0).round() as i64,
    );
    let cache = CACHE.get_or_init(|| Mutex::new(HashMap::new()));
    if let Ok(cache) = cache.lock() {
        if let Some(value) = cache.get(&key) {
            return value.clone();
        }
    }

    let finder = FINDER.get_or_init(DefaultFinder::new);
    let name = finder.get_tz_name(lon, lat);
    let value = if name.is_empty() || name == "Etc/GMT" {
        None
    } else {
        Some(name.to_string())
    };
    if let Ok(mut cache) = cache.lock() {
        cache.insert(key, value.clone());
    }
    value
}

#[cfg(test)]
mod tests {
    use super::timezone_name;

    #[test]
    fn resolves_common_city_timezones() {
        assert_eq!(
            timezone_name(104.7612, -2.9055).as_deref(),
            Some("Asia/Jakarta")
        );
        assert_eq!(
            timezone_name(-0.1278, 51.5074).as_deref(),
            Some("Europe/London")
        );
        assert_eq!(
            timezone_name(-74.0060, 40.7128).as_deref(),
            Some("America/New_York")
        );
    }
}
