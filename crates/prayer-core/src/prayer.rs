//! Prayer-time engine ported from ITL (Islamic Tools and Libraries).
//!
//! Original C code: Copyright (c) 2003-2006, 2009 Arabeyes,
//! Thamer Mahmoud. LGPL-2.1. See reference/itl/COPYING.

use crate::astro::{
    deg_to_rad, get_astro_values_by_day, get_julian_day, get_sunrise, get_sunset, get_transit,
    rad_to_deg, Astro, DEG_TO_10_BASE, INVALID_TRIGGER,
};
use crate::{Date, Location, Method, Prayer};

pub const KAABA_LAT: f64 = 21.423333;
pub const KAABA_LONG: f64 = 39.823333;
pub const DEF_NEAREST_LATITUDE: f64 = 48.5;
pub const DEF_EXTREME_LATITUDE: f64 = 55.0;
pub const DEF_IMSAAK_ANGLE: f64 = 1.5;
pub const DEF_IMSAAK_INTERVAL: i32 = 10;
pub const DEF_ROUND_SEC: f64 = 30.0;
pub const AGGRESSIVE_ROUND_SEC: f64 = 1.0;

pub const FAJR: usize = 0;
pub const SHUROOQ: usize = 1;
pub const ZUHR: usize = 2;
pub const ASSR: usize = 3;
pub const MAGHRIB: usize = 4;
pub const ISHAA: usize = 5;
pub const IMSAAK: usize = 6;
pub const NEXTFAJR: usize = 7;

pub const NONE_EX: i32 = 0;
pub const LAT_ALL: i32 = 1;
pub const LAT_ALWAYS: i32 = 2;
pub const LAT_INVALID: i32 = 3;
pub const GOOD_ALL: i32 = 4;
pub const GOOD_INVALID: i32 = 5;
pub const SEVEN_NIGHT_ALWAYS: i32 = 6;
pub const SEVEN_NIGHT_INVALID: i32 = 7;
pub const SEVEN_DAY_ALWAYS: i32 = 8;
pub const SEVEN_DAY_INVALID: i32 = 9;
pub const HALF_ALWAYS: i32 = 10;
pub const HALF_INVALID: i32 = 11;
pub const MIN_ALWAYS: i32 = 12;
pub const MIN_INVALID: i32 = 13;
pub const GOOD_INVALID_SAME: i32 = 14;
pub const ANGLE_BASED: i32 = 15;

pub const NONE: i32 = 0;
pub const EGYPT_SURVEY: i32 = 1;
pub const KARACHI_SHAF: i32 = 2;
pub const KARACHI_HANAF: i32 = 3;
pub const NORTH_AMERICA: i32 = 4;
pub const MUSLIM_LEAGUE: i32 = 5;
pub const UMM_ALQURRA: i32 = 6;
pub const FIXED_ISHAA: i32 = 7;
pub const EGYPT_NEW: i32 = 8;
pub const UMM_ALQURRA_RAMADAN: i32 = 9;
pub const MOONSIGHTING_COMMITTEE: i32 = 10;
pub const MOROCCO_AWQAF: i32 = 11;

pub fn get_method(n: i32) -> Method {
    let mut conf = Method {
        method: n,
        fajr_ang: 0.0,
        ishaa_ang: 0.0,
        imsaak_ang: DEF_IMSAAK_ANGLE,
        fajr_inv: 0,
        ishaa_inv: 0,
        imsaak_inv: 0,
        round: 2,
        mathhab: 1,
        nearest_lat: DEF_NEAREST_LATITUDE,
        extreme: GOOD_INVALID,
        extreme_lat: DEF_EXTREME_LATITUDE,
        offset: 0,
        off_list: [0.0; 6],
    };
    match n {
        NONE => {}
        EGYPT_SURVEY => {
            conf.fajr_ang = 20.0;
            conf.ishaa_ang = 18.0;
        }
        KARACHI_SHAF => {
            conf.fajr_ang = 18.0;
            conf.ishaa_ang = 18.0;
        }
        KARACHI_HANAF => {
            conf.fajr_ang = 18.0;
            conf.ishaa_ang = 18.0;
            conf.mathhab = 2;
        }
        NORTH_AMERICA => {
            conf.fajr_ang = 15.0;
            conf.ishaa_ang = 15.0;
        }
        MUSLIM_LEAGUE => {
            conf.fajr_ang = 18.0;
            conf.ishaa_ang = 17.0;
        }
        UMM_ALQURRA => {
            conf.fajr_ang = 18.0;
            conf.ishaa_ang = 0.0;
            conf.ishaa_inv = 90;
        }
        FIXED_ISHAA => {
            conf.fajr_ang = 19.5;
            conf.ishaa_ang = 0.0;
            conf.ishaa_inv = 90;
        }
        EGYPT_NEW => {
            conf.fajr_ang = 19.5;
            conf.ishaa_ang = 17.5;
        }
        UMM_ALQURRA_RAMADAN => {
            conf.fajr_ang = 18.0;
            conf.ishaa_ang = 0.0;
            conf.ishaa_inv = 120;
        }
        MOONSIGHTING_COMMITTEE => {
            conf.fajr_ang = 18.0;
            conf.ishaa_ang = 18.0;
        }
        MOROCCO_AWQAF => {
            conf.fajr_ang = 19.0;
            conf.ishaa_ang = 17.0;
            conf.offset = 1;
            conf.off_list[ZUHR] = 5.0;
            conf.off_list[MAGHRIB] = 5.0;
        }
        _ => {}
    }
    conf
}

fn get_day_info(date: &Date, gmt: f64) -> (i32, i32, f64) {
    let last_day = get_day_of_year(date.year, 12, 31);
    let day_of_year = get_day_of_year(date.year, date.month, date.day);
    let julian_day = get_julian_day(date, gmt);
    (last_day, day_of_year, julian_day)
}

pub fn get_day_of_year(year: i32, month: i32, mut day: i32) -> i32 {
    let leap = (year & 3) == 0 && (year % 100 != 0 || year % 400 == 0);
    let day_list: [[i32; 13]; 2] = [
        [0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31],
        [0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31],
    ];
    for i in 1..month {
        day += day_list[leap as usize][i as usize];
    }
    day
}

pub fn dms2decimal(deg: i32, min: i32, sec: f64, dir: char) -> f64 {
    let sum = deg as f64 + (min as f64 / 60.0) + (sec / 3600.0);
    if matches!(dir, 'S' | 'W' | 's' | 'w') {
        -sum
    } else {
        sum
    }
}

pub fn decimal2dms(decimal: f64) -> (i32, i32, f64) {
    let n1 = decimal.trunc();
    let tempmin = (decimal - n1) * 60.0;
    let n2 = tempmin.trunc();
    let tempsec = (tempmin - n2) * 60.0;
    (n1 as i32, n2 as i32, tempsec)
}

fn get_faj_ish(lat: f64, dec: f64, ang: f64) -> f64 {
    let rlat = deg_to_rad(lat);
    let part1 = deg_to_rad(-ang).sin() - (rlat.sin() * dec.sin());
    let part2 = rlat.cos() * dec.cos();
    let part3 = part1 / part2;
    if !(-INVALID_TRIGGER..=INVALID_TRIGGER).contains(&part3) {
        return 99.0;
    }
    DEG_TO_10_BASE * rad_to_deg(part3.acos())
}

fn get_assr(lat: f64, dec: f64, mathhab: i32) -> f64 {
    let rlat = deg_to_rad(lat);
    let part1 = mathhab as f64 + (rlat - dec).abs().tan();
    let part2 = (1.0 / part1).atan();
    let part3 = part2.sin() - (rlat.sin() * dec.sin());
    let part4 = part3 / (rlat.cos() * dec.cos());
    if !(-INVALID_TRIGGER..=INVALID_TRIGGER).contains(&part4) {
        return 99.0;
    }
    DEG_TO_10_BASE * rad_to_deg(part4.acos())
}

fn get_season_day(day_of_year: i32, lat: f64) -> i32 {
    let mut season_day;
    if lat >= 0.0 {
        season_day = day_of_year + 10;
        if season_day > 365 {
            season_day -= 365;
        }
    } else {
        season_day = day_of_year - 172;
        if season_day < 0 {
            season_day += 365;
        }
    }
    season_day
}

fn get_seasonal_fajr(lat: f64, day: i32, mut fajr: f64, sunrise: f64) -> f64 {
    let dyy = get_season_day(day, lat);
    let mut a = 75.0 + 28.65 / 55.0 * lat.abs();
    let b = 75.0 + 19.44 / 55.0 * lat.abs();
    let c = 75.0 + 32.74 / 55.0 * lat.abs();
    let d = 75.0 + 48.1 / 55.0 * lat.abs();
    if dyy < 91 {
        a += (b - a) / 91.0 * dyy as f64;
    } else if dyy < 137 {
        a = b + (c - b) / 46.0 * (dyy - 91) as f64;
    } else if dyy < 183 {
        a = c + (d - c) / 46.0 * (dyy - 137) as f64;
    } else if dyy < 229 {
        a = d + (c - d) / 46.0 * (dyy - 183) as f64;
    } else if dyy < 275 {
        a = c + (b - c) / 46.0 * (dyy - 229) as f64;
    } else {
        a = b + (a - b) / 91.0 * (dyy - 275) as f64;
    }
    let adjusted = sunrise - (a.floor() / 60.0);
    if adjusted > fajr || fajr == 99.0 {
        fajr = adjusted;
    }
    fajr
}

fn get_seasonal_isha(lat: f64, day: i32, mut isha: f64, sunset: f64) -> f64 {
    let dyy = get_season_day(day, lat);
    let mut a = 75.0 + 25.6 / 55.0 * lat.abs();
    let b = 75.0 + 2.05 / 55.0 * lat.abs();
    let c = 75.0 - 9.21 / 55.0 * lat.abs();
    let d = 75.0 + 6.14 / 55.0 * lat.abs();
    if dyy < 91 {
        a += (b - a) / 91.0 * dyy as f64;
    } else if dyy < 137 {
        a = b + (c - b) / 46.0 * (dyy - 91) as f64;
    } else if dyy < 183 {
        a = c + (d - c) / 46.0 * (dyy - 137) as f64;
    } else if dyy < 229 {
        a = d + (c - d) / 46.0 * (dyy - 183) as f64;
    } else if dyy < 275 {
        a = c + (b - c) / 46.0 * (dyy - 229) as f64;
    } else {
        a = b + (a - b) / 91.0 * (dyy - 275) as f64;
    }
    let adjusted = sunset + (a.ceil() / 60.0);
    if adjusted < isha || isha == 99.0 {
        isha = adjusted;
    }
    isha
}

pub fn get_prayer_times(loc: &Location, conf: &Method, date: &Date) -> [Prayer; 6] {
    let (last_day, day_of_year, julian_day) = get_day_info(date, loc.gmt_diff);
    get_prayer_times_by_day(loc, conf, last_day, day_of_year, julian_day, 0)
}

fn get_prayer_times_by_day(
    loc: &Location,
    conf: &Method,
    last_day: i32,
    day_of_year: i32,
    julian_day: f64,
    kind: usize,
) -> [Prayer; 6] {
    let mut pt = [Prayer::default(); 6];
    let lat = loc.degree_lat;
    let lon = loc.degree_long;
    let mut invalid = false;

    let mut astro_cache = Astro::default();
    let mut t_astro = Astro::default();
    get_astro_values_by_day(julian_day, loc, &mut astro_cache, &mut t_astro);
    let dec = deg_to_rad(t_astro.dec[1]);

    let mut temp_prayer = [0.0f64; 6];
    let fj = get_faj_ish(lat, dec, conf.fajr_ang);
    let sh = get_sunrise(loc, &t_astro);
    let zu = get_transit(lon, &t_astro);
    let ar = get_assr(lat, dec, conf.mathhab);
    let mg = get_sunset(loc, &t_astro);
    let is = get_faj_ish(lat, dec, conf.ishaa_ang);

    if fj == 99.0 {
        temp_prayer[FAJR] = 99.0;
        if conf.method != MOONSIGHTING_COMMITTEE {
            invalid = true;
        }
    } else {
        temp_prayer[FAJR] = zu - fj;
    }
    if sh == 99.0 {
        invalid = true;
    }
    temp_prayer[SHUROOQ] = sh;
    temp_prayer[ZUHR] = zu;
    if ar == 99.0 {
        temp_prayer[ASSR] = 99.0;
        invalid = true;
    } else {
        temp_prayer[ASSR] = zu + ar;
    }
    if mg == 99.0 {
        invalid = true;
    }
    temp_prayer[MAGHRIB] = mg;
    if is == 99.0 {
        temp_prayer[ISHAA] = 99.0;
        if conf.method != MOONSIGHTING_COMMITTEE {
            invalid = true;
        }
    } else {
        temp_prayer[ISHAA] = zu + is;
    }

    if conf.method == MOONSIGHTING_COMMITTEE {
        temp_prayer[FAJR] =
            get_seasonal_fajr(lat, day_of_year, temp_prayer[FAJR], temp_prayer[SHUROOQ]);
        temp_prayer[ISHAA] =
            get_seasonal_isha(lat, day_of_year, temp_prayer[ISHAA], temp_prayer[MAGHRIB]);
        if temp_prayer[FAJR] == 99.0 || temp_prayer[ISHAA] == 99.0 {
            invalid = true;
        }
        if temp_prayer[ZUHR] != 99.0 {
            temp_prayer[ZUHR] += 5.0 / 60.0;
        }
        if temp_prayer[MAGHRIB] != 99.0 {
            temp_prayer[MAGHRIB] += 3.0 / 60.0;
        }
    }

    if lat > conf.extreme_lat {
        temp_prayer[FAJR] = 99.0;
        temp_prayer[ISHAA] = 99.0;
        invalid = true;
    }

    for p in pt.iter_mut() {
        p.is_extreme = 0;
    }

    let always_method = matches!(
        conf.extreme,
        LAT_ALL
            | LAT_ALWAYS
            | GOOD_ALL
            | SEVEN_NIGHT_ALWAYS
            | SEVEN_DAY_ALWAYS
            | HALF_ALWAYS
            | MIN_ALWAYS
    );
    if conf.extreme != NONE_EX && (invalid || always_method) {
        let mut ex_zu = 99.0;
        let mut ex_fj = 99.0;
        let mut ex_is = 99.0;
        let mut ex_ar = 99.0;
        let mut ex_sh = 99.0;
        let mut ex_mg = 99.0;
        let ex_loc;
        let mut ex_astro_prev;
        let mut ex_astro_next;

        match conf.extreme {
            ANGLE_BASED => {
                let portion = (24.0 - temp_prayer[MAGHRIB]) + temp_prayer[SHUROOQ];
                let fajr_diff = (1.0 / 60.0 * conf.fajr_ang) * portion;
                let isha_diff = (1.0 / 60.0 * conf.ishaa_ang) * portion;
                temp_prayer[FAJR] = temp_prayer[SHUROOQ] - fajr_diff;
                temp_prayer[ISHAA] = temp_prayer[MAGHRIB] + isha_diff;
                pt[FAJR].is_extreme = 1;
                pt[ISHAA].is_extreme = 1;
            }
            LAT_ALL | LAT_ALWAYS | LAT_INVALID => {
                ex_loc = Location {
                    degree_lat: conf.nearest_lat,
                    ..*loc
                };
                ex_fj = get_faj_ish(conf.nearest_lat, dec, conf.fajr_ang);
                ex_sh = get_sunrise(&ex_loc, &t_astro);
                ex_ar = get_assr(conf.nearest_lat, dec, conf.mathhab);
                ex_mg = get_sunset(&ex_loc, &t_astro);
                ex_is = get_faj_ish(conf.nearest_lat, dec, conf.ishaa_ang);
                match conf.extreme {
                    LAT_ALL => {
                        temp_prayer[FAJR] = zu - ex_fj;
                        temp_prayer[SHUROOQ] = ex_sh;
                        temp_prayer[ASSR] = zu + ex_ar;
                        temp_prayer[MAGHRIB] = ex_mg;
                        temp_prayer[ISHAA] = zu + ex_is;
                        for p in pt.iter_mut() {
                            p.is_extreme = 1;
                        }
                    }
                    LAT_ALWAYS => {
                        temp_prayer[FAJR] = zu - ex_fj;
                        temp_prayer[ISHAA] = zu + ex_is;
                        pt[FAJR].is_extreme = 1;
                        pt[ISHAA].is_extreme = 1;
                    }
                    LAT_INVALID => {
                        if temp_prayer[FAJR] == 99.0 {
                            temp_prayer[FAJR] = zu - ex_fj;
                            pt[FAJR].is_extreme = 1;
                        }
                        if temp_prayer[ISHAA] == 99.0 {
                            temp_prayer[ISHAA] = zu + ex_is;
                            pt[ISHAA].is_extreme = 1;
                        }
                    }
                    _ => {}
                }
            }
            GOOD_ALL | GOOD_INVALID | GOOD_INVALID_SAME => {
                ex_astro_prev = astro_cache;
                ex_astro_next = astro_cache;
                for i in 0..=last_day {
                    let n_good_day = julian_day - i as f64;
                    let mut t = Astro::default();
                    get_astro_values_by_day(n_good_day, loc, &mut ex_astro_prev, &mut t);
                    let ex_dec_prev = deg_to_rad(t.dec[1]);
                    ex_fj = get_faj_ish(lat, ex_dec_prev, conf.fajr_ang);
                    if ex_fj != 99.0 {
                        ex_is = get_faj_ish(lat, ex_dec_prev, conf.ishaa_ang);
                        if ex_is != 99.0 {
                            ex_zu = get_transit(lon, &t);
                            ex_sh = get_sunrise(loc, &t);
                            ex_ar = get_assr(lat, ex_dec_prev, conf.mathhab);
                            ex_mg = get_sunset(loc, &t);
                            break;
                        }
                    }
                    let n_good_day = julian_day + i as f64;
                    let mut t2 = Astro::default();
                    get_astro_values_by_day(n_good_day, loc, &mut ex_astro_next, &mut t2);
                    let ex_dec_next = deg_to_rad(t2.dec[1]);
                    ex_fj = get_faj_ish(lat, ex_dec_next, conf.fajr_ang);
                    if ex_fj != 99.0 {
                        ex_is = get_faj_ish(lat, ex_dec_next, conf.ishaa_ang);
                        if ex_is != 99.0 {
                            ex_zu = get_transit(lon, &t2);
                            ex_sh = get_sunrise(loc, &t2);
                            ex_ar = get_assr(lat, ex_dec_next, conf.mathhab);
                            ex_mg = get_sunset(loc, &t2);
                            break;
                        }
                    }
                }
                match conf.extreme {
                    GOOD_ALL => {
                        temp_prayer[FAJR] = ex_zu - ex_fj;
                        temp_prayer[SHUROOQ] = ex_sh;
                        temp_prayer[ZUHR] = ex_zu;
                        temp_prayer[ASSR] = ex_zu + ex_ar;
                        temp_prayer[MAGHRIB] = ex_mg;
                        temp_prayer[ISHAA] = ex_zu + ex_is;
                        for p in pt.iter_mut() {
                            p.is_extreme = 1;
                        }
                    }
                    GOOD_INVALID => {
                        if temp_prayer[FAJR] == 99.0 {
                            temp_prayer[FAJR] = ex_zu - ex_fj;
                            pt[FAJR].is_extreme = 1;
                        }
                        if temp_prayer[ISHAA] == 99.0 {
                            temp_prayer[ISHAA] = ex_zu + ex_is;
                            pt[ISHAA].is_extreme = 1;
                        }
                    }
                    GOOD_INVALID_SAME => {
                        if temp_prayer[FAJR] == 99.0 || temp_prayer[ISHAA] == 99.0 {
                            temp_prayer[FAJR] = ex_zu - ex_fj;
                            pt[FAJR].is_extreme = 1;
                            temp_prayer[ISHAA] = ex_zu + ex_is;
                            pt[ISHAA].is_extreme = 1;
                        }
                    }
                    _ => {}
                }
            }
            SEVEN_NIGHT_ALWAYS | SEVEN_NIGHT_INVALID | SEVEN_DAY_ALWAYS | SEVEN_DAY_INVALID
            | HALF_ALWAYS | HALF_INVALID => {
                let portion = match conf.extreme {
                    SEVEN_NIGHT_ALWAYS | SEVEN_NIGHT_INVALID => {
                        (24.0 - (temp_prayer[MAGHRIB] - temp_prayer[SHUROOQ])) * (1.0 / 7.0)
                    }
                    SEVEN_DAY_ALWAYS | SEVEN_DAY_INVALID => {
                        (temp_prayer[MAGHRIB] - temp_prayer[SHUROOQ]) * (1.0 / 7.0)
                    }
                    _ => (24.0 - temp_prayer[MAGHRIB] - temp_prayer[SHUROOQ]) * 0.5,
                };
                if matches!(
                    conf.extreme,
                    SEVEN_NIGHT_INVALID | SEVEN_DAY_INVALID | HALF_INVALID
                ) {
                    if temp_prayer[FAJR] == 99.0 {
                        if conf.extreme == HALF_INVALID {
                            temp_prayer[FAJR] = portion - (conf.fajr_inv as f64 / 60.0);
                        } else {
                            temp_prayer[FAJR] = temp_prayer[SHUROOQ] - portion;
                        }
                        pt[FAJR].is_extreme = 1;
                    }
                    if temp_prayer[ISHAA] == 99.0 {
                        if conf.extreme == HALF_INVALID {
                            temp_prayer[ISHAA] = portion + (conf.ishaa_inv as f64 / 60.0);
                        } else {
                            temp_prayer[ISHAA] = temp_prayer[MAGHRIB] + portion;
                        }
                        pt[ISHAA].is_extreme = 1;
                    }
                } else {
                    if conf.extreme == HALF_ALWAYS {
                        temp_prayer[FAJR] = portion - (conf.fajr_inv as f64 / 60.0);
                        temp_prayer[ISHAA] = portion + (conf.ishaa_inv as f64 / 60.0);
                    } else {
                        temp_prayer[FAJR] = temp_prayer[SHUROOQ] - portion;
                        temp_prayer[ISHAA] = temp_prayer[MAGHRIB] + portion;
                    }
                    pt[FAJR].is_extreme = 1;
                    pt[ISHAA].is_extreme = 1;
                }
            }
            MIN_ALWAYS => {
                temp_prayer[FAJR] = temp_prayer[SHUROOQ];
                temp_prayer[ISHAA] = temp_prayer[MAGHRIB];
                pt[FAJR].is_extreme = 1;
                pt[ISHAA].is_extreme = 1;
            }
            MIN_INVALID => {
                if temp_prayer[FAJR] == 99.0 {
                    let ex_interval = conf.fajr_inv / 60;
                    temp_prayer[FAJR] = temp_prayer[SHUROOQ] - ex_interval as f64;
                    pt[FAJR].is_extreme = 1;
                }
                if temp_prayer[ISHAA] == 99.0 {
                    let ex_interval = conf.ishaa_inv / 60;
                    temp_prayer[ISHAA] = temp_prayer[MAGHRIB] + ex_interval as f64;
                    pt[ISHAA].is_extreme = 1;
                }
            }
            _ => {}
        }
    }

    if conf.extreme != MIN_INVALID && conf.extreme != HALF_INVALID && conf.extreme != HALF_ALWAYS {
        if conf.fajr_inv != 0 {
            if temp_prayer[SHUROOQ] != 99.0 {
                temp_prayer[FAJR] = temp_prayer[SHUROOQ] - (conf.fajr_inv as f64 / 60.0);
            } else {
                temp_prayer[FAJR] = 99.0;
            }
        }
        if conf.ishaa_inv != 0 {
            if temp_prayer[MAGHRIB] != 99.0 {
                temp_prayer[ISHAA] = temp_prayer[MAGHRIB] + (conf.ishaa_inv as f64 / 60.0);
            } else {
                temp_prayer[ISHAA] = 99.0;
            }
        }
    }

    if kind == IMSAAK || kind == NEXTFAJR {
        base6hm(temp_prayer[FAJR], loc, conf, &mut pt[0], kind);
    } else {
        for i in 0..6 {
            base6hm(temp_prayer[i], loc, conf, &mut pt[i], i);
        }
    }
    pt
}

fn base6hm(bs_in: f64, loc: &Location, conf: &Method, pt: &mut Prayer, kind: usize) {
    if bs_in == 99.0 {
        pt.hour = 99;
        pt.minute = 99;
        pt.second = 0;
        return;
    }
    let mut bs = bs_in;
    if conf.offset == 1 {
        if kind == IMSAAK || kind == NEXTFAJR {
            bs += conf.off_list[0] / 60.0;
        } else {
            bs += conf.off_list[kind] / 60.0;
        }
    }
    if bs < 0.0 {
        while bs < 0.0 {
            bs += 24.0;
        }
    }
    let mut min = (bs - bs.floor()) * 60.0;
    let mut sec = (min - min.floor()) * 60.0;
    if conf.round == 1 {
        if sec >= DEF_ROUND_SEC {
            bs += 1.0 / 60.0;
        }
        min = (bs - bs.floor()) * 60.0;
        sec = 0.0;
    } else if conf.round == 2 || conf.round == 3 {
        match kind {
            FAJR | ZUHR | ASSR | MAGHRIB | ISHAA | NEXTFAJR => {
                if conf.round == 2 {
                    if sec >= DEF_ROUND_SEC {
                        bs += 1.0 / 60.0;
                        min = (bs - bs.floor()) * 60.0;
                    }
                } else if sec >= AGGRESSIVE_ROUND_SEC {
                    bs += 1.0 / 60.0;
                    min = (bs - bs.floor()) * 60.0;
                }
                sec = 0.0;
            }
            SHUROOQ | IMSAAK => {
                sec = 0.0;
            }
            _ => {}
        }
    }
    bs += loc.dst as f64;
    if bs >= 24.0 {
        bs %= 24.0;
    }
    pt.hour = bs as i32;
    pt.minute = min as i32;
    pt.second = sec as i32;
}

pub fn get_imsaak(loc: &Location, conf: &Method, date: &Date) -> Prayer {
    let mut tmp = *conf;
    if conf.fajr_inv != 0 {
        if conf.imsaak_inv == 0 {
            tmp.fajr_inv += DEF_IMSAAK_INTERVAL;
        } else {
            tmp.fajr_inv += conf.imsaak_inv;
        }
    } else if conf.imsaak_inv != 0 {
        tmp.off_list[0] += (-conf.imsaak_inv) as f64;
        tmp.offset = 1;
    } else {
        tmp.fajr_ang += conf.imsaak_ang;
    }
    let (last_day, day_of_year, julian_day) = get_day_info(date, loc.gmt_diff);
    let mut temp = get_prayer_times_by_day(loc, &tmp, last_day, day_of_year, julian_day, IMSAAK);
    if temp[0].is_extreme != 0 {
        tmp = *conf;
        if conf.imsaak_inv == 0 {
            tmp.off_list[0] -= DEF_IMSAAK_INTERVAL as f64;
            tmp.offset = 1;
        } else {
            tmp.off_list[0] -= conf.imsaak_inv as f64;
            tmp.offset = 1;
        }
        temp = get_prayer_times_by_day(loc, &tmp, last_day, day_of_year, julian_day, IMSAAK);
    }
    temp[0]
}

pub fn get_next_day_imsaak(loc: &Location, conf: &Method, date: &Date) -> Prayer {
    let mut d = *date;
    d.day += 1;
    get_imsaak(loc, conf, &d)
}

pub fn get_next_day_fajr(loc: &Location, conf: &Method, date: &Date) -> Prayer {
    let (last_day, day_of_year, julian_day) = get_day_info(date, loc.gmt_diff);
    let temp = get_prayer_times_by_day(
        loc,
        conf,
        last_day,
        day_of_year + 1,
        julian_day + 1.0,
        NEXTFAJR,
    );
    temp[0]
}

/// Direction of the shortest great-circle path to the Kaaba.
pub fn get_north_qibla(loc: &Location) -> f64 {
    let num = (deg_to_rad(loc.degree_long) - deg_to_rad(KAABA_LONG)).sin();
    let denom = (deg_to_rad(loc.degree_lat).cos() * deg_to_rad(KAABA_LAT).tan())
        - (deg_to_rad(loc.degree_lat).sin()
            * (deg_to_rad(loc.degree_long) - deg_to_rad(KAABA_LONG)).cos());
    rad_to_deg(num.atan2(denom))
}
