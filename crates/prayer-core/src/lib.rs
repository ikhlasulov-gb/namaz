//! Pure-Rust port of the ITL prayer-time engine used by Namaz.
//!
//! This crate is intentionally UI-independent so the original Windows DLL can
//! be used as a golden oracle while the Linux application is developed.

#![allow(clippy::approx_constant)]
#![allow(clippy::collapsible_match)]

pub mod astro;
pub mod prayer;

mod tables_b0;
mod tables_b1;
mod tables_coeff;
mod tables_l0;
mod tables_l1;
mod tables_l2;
mod tables_l3;
mod tables_l4;
mod tables_l5;
mod tables_pn;
mod tables_r0;
mod tables_r1;
mod tables_r2;
mod tables_r3;
mod tables_r4;

pub use prayer::*;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Date {
    pub day: i32,
    pub month: i32,
    pub year: i32,
}

impl Date {
    pub fn new(year: i32, month: i32, day: i32) -> Self {
        Self { day, month, year }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Location {
    /// Longitude in decimal degrees, east positive.
    pub degree_long: f64,
    /// Latitude in decimal degrees, north positive.
    pub degree_lat: f64,
    /// GMT offset at regular time, in hours.
    pub gmt_diff: f64,
    /// Daylight-saving offset in hours (0 or 1 for the original app).
    pub dst: i32,
    /// Height above sea level in metres.
    pub sea_level: f64,
    /// Atmospheric pressure in millibars.
    pub pressure: f64,
    /// Temperature in Celsius.
    pub temperature: f64,
}

impl Default for Location {
    fn default() -> Self {
        Self {
            degree_long: 0.0,
            degree_lat: 0.0,
            gmt_diff: 0.0,
            dst: 0,
            sea_level: 0.0,
            pressure: 1010.0,
            temperature: 10.0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Method {
    pub method: i32,
    pub fajr_ang: f64,
    pub ishaa_ang: f64,
    pub imsaak_ang: f64,
    pub fajr_inv: i32,
    pub ishaa_inv: i32,
    pub imsaak_inv: i32,
    pub round: i32,
    pub mathhab: i32,
    pub nearest_lat: f64,
    pub extreme: i32,
    pub extreme_lat: f64,
    pub offset: i32,
    pub off_list: [f64; 6],
}

impl Default for Method {
    fn default() -> Self {
        prayer::get_method(0)
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Prayer {
    pub hour: i32,
    pub minute: i32,
    pub second: i32,
    pub is_extreme: i32,
}

impl Prayer {
    /// `HH:MM:SS`, or `--:--:--` when the original engine returns 99:99.
    pub fn hms(&self) -> String {
        if self.hour == 99 {
            "--:--:--".to_string()
        } else {
            format!("{:02}:{:02}:{:02}", self.hour, self.minute, self.second)
        }
    }

    pub fn seconds_of_day(&self) -> Option<u32> {
        if self.hour == 99 {
            None
        } else {
            Some(
                (self.hour.max(0) as u32) * 3600
                    + (self.minute.max(0) as u32) * 60
                    + self.second.max(0) as u32,
            )
        }
    }
}
