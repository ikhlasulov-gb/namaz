//! Astronomical routines ported from ITL (Islamic Tools and Libraries).
//!
//! Original C code: Copyright (c) 2003-2006, 2009-2010 Arabeyes,
//! Thamer Mahmoud. LGPL-2.1. See reference/itl/COPYING.
//!
//! The formulas are based on Jean Meeus, *Astronomical Algorithms*,
//! 2nd edition, Willmann-Bell, 1998.

use crate::tables_b0::B0;
use crate::tables_b1::B1;
use crate::tables_coeff::COEFF;
use crate::tables_l0::L0;
use crate::tables_l1::L1;
use crate::tables_l2::L2;
use crate::tables_l3::L3;
use crate::tables_l4::L4;
use crate::tables_l5::L5;
use crate::tables_pn::PN;
use crate::tables_r0::R0;
use crate::tables_r1::R1;
use crate::tables_r2::R2;
use crate::tables_r3::R3;
use crate::tables_r4::R4;
use crate::{Date, Location};

pub const PI: f64 = 3.1415926535898;
pub const INVALID_TRIGGER: f64 = 1.0;
pub const CENTER_OF_SUN_ANGLE: f64 = -0.833370;
pub const EARTH_RADIUS: f64 = 6_378_140.0;
pub const ALTITUDE_REFRACTION: f64 = 0.0347;
pub const DEG_TO_10_BASE: f64 = 1.0 / 15.0;

#[inline]
pub fn deg_to_rad(a: f64) -> f64 {
    a * (PI / 180.0)
}

#[inline]
pub fn rad_to_deg(a: f64) -> f64 {
    a / (PI / 180.0)
}

#[derive(Debug, Clone, Copy, Default)]
pub struct Astro {
    pub jd: f64,
    pub dec: [f64; 3],
    pub ra: [f64; 3],
    pub sid: [f64; 3],
    pub dra: [f64; 3],
    pub rsum: [f64; 3],
}

#[derive(Debug, Clone, Copy, Default)]
struct AstroDay {
    dec: f64,
    ra: f64,
    sidtime: f64,
    dra: f64,
    rsum: f64,
}

const DT2: [f64; 19] = [
    63.4673, 63.8285, 64.0908, 64.2998, 64.4734, 64.5736, 64.7052, 64.8452, 65.1464, 65.4574,
    65.7768, 66.5, 67.1, 68.0, 68.0, 69.0, 69.0, 70.0, 70.0,
];

fn compute_delta_t(year: i32) -> f64 {
    let t = (year as f64 - 2000.0) / 100.0;
    if year < 948 {
        return 2177.0 + (497.0 * t) + (44.1 * t.powf(2.0));
    } else if (1620..=1998).contains(&year) {
        return 0.0;
    } else if (1999..2100).contains(&year) || year < 1620 {
        if (1999..=2017).contains(&year) {
            return DT2[(year - 1999) as usize];
        }
        let mut tempdt = 102.0 + (102.0 * t) + (25.3 * t.powf(2.0));
        if year >= 2000 {
            tempdt += 0.37 * (year as f64 - 2100.0);
        }
        return tempdt;
    }
    0.0
}

pub fn get_julian_day(date: &Date, gmt: f64) -> f64 {
    let mut jd_y = date.year as f64;
    let mut jd_m = date.month as f64;

    if date.month <= 2 {
        jd_y -= 1.0;
        jd_m += 12.0;
    }
    if date.year < 1 {
        jd_y += 1.0;
    }

    let jd_b = if (date.year > 1582)
        || (date.year == 1582 && (date.month > 10 || (date.month == 10 && date.day >= 4)))
    {
        2.0 - (jd_y / 100.0).floor() + ((jd_y / 100.0) / 4.0).floor()
    } else {
        0.0
    };

    let mut jd = (365.25 * (jd_y + 4716.0)).floor()
        + (30.6001 * (jd_m + 1.0)).floor()
        + (date.day as f64 + (-gmt) / 24.0)
        + jd_b
        - 1524.5;
    jd += compute_delta_t(date.year) / 86400.0;
    jd
}

fn limit_angle(mut l: f64) -> f64 {
    l /= 360.0;
    let f = l - l.floor();
    if f > 0.0 {
        360.0 * f
    } else if f < 0.0 {
        360.0 - 360.0 * f
    } else {
        l
    }
}

fn limit_angle_180(mut l: f64) -> f64 {
    l /= 180.0;
    let f = l - l.floor();
    if f > 0.0 {
        180.0 * f
    } else if f < 0.0 {
        180.0 - 180.0 * f
    } else {
        l
    }
}

fn limit_angle_1(l: f64) -> f64 {
    let mut f = l - l.floor();
    if f < 0.0 {
        f += 1.0;
    }
    f
}

fn limit_angle_180_between(mut l: f64) -> f64 {
    l /= 360.0;
    let mut f = (l - l.floor()) * 360.0;
    if f < -180.0 {
        f += 360.0;
    } else if f > 180.0 {
        f -= 360.0;
    }
    f
}

fn compute_astro_day(jd: f64) -> AstroDay {
    let jc = (jd - 2_451_545.0) / 36525.0;
    let jm = jc / 10.0;
    let jm2 = jm.powf(2.0);
    let jm3 = jm.powf(3.0);
    let jm4 = jm.powf(4.0);
    let jm5 = jm.powf(5.0);
    let jde = jd;
    let t = (jde - 2_451_545.0) / 36525.0;

    let mut l0sum = 0.0;
    let mut l1sum = 0.0;
    let mut l2sum = 0.0;
    let mut l3sum = 0.0;
    let mut l4sum = 0.0;
    let mut b0sum = 0.0;
    let mut b1sum = 0.0;
    let mut r0sum = 0.0;
    let mut r1sum = 0.0;
    let mut r2sum = 0.0;
    let mut r3sum = 0.0;

    for row in L0.iter() {
        l0sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    for row in L1.iter() {
        l1sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    for row in L2.iter() {
        l2sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    for row in L3.iter() {
        l3sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    for row in L4.iter() {
        l4sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    let l5sum = L5[0][0] * (L5[0][1] + L5[0][2] * jm).cos();

    let tl = (l0sum + (l1sum * jm) + (l2sum * jm2) + (l3sum * jm3) + (l4sum * jm4) + (l5sum * jm5))
        / 10f64.powi(8);
    let l = limit_angle(rad_to_deg(tl));

    for row in B0.iter() {
        b0sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    for row in B1.iter() {
        b1sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    let tb = (b0sum + (b1sum * jm)) / 10f64.powi(8);
    let b = rad_to_deg(tb);

    for row in R0.iter() {
        r0sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    for row in R1.iter() {
        r1sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    for row in R2.iter() {
        r2sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    for row in R3.iter() {
        r3sum += row[0] * (row[1] + row[2] * jm).cos();
    }
    let r4sum = R4[0][0] * (R4[0][1] + R4[0][2] * jm).cos();

    let r = (r0sum + (r1sum * jm) + (r2sum * jm2) + (r3sum * jm3) + (r4sum * jm4)) / 10f64.powi(8);
    let g = limit_angle(l + 180.0);
    let gg = -b;
    let rgg = deg_to_rad(gg);

    let d = 297.85036 + (445267.111480 * t) - (0.0019142 * t.powf(2.0)) + (t.powf(3.0) / 189474.0);
    let m = 357.52772 + (35999.050340 * t) - (0.0001603 * t.powf(2.0)) - (t.powf(3.0) / 300000.0);
    let m1 = 134.96298 + (477198.867398 * t) + (0.0086972 * t.powf(2.0)) + (t.powf(3.0) / 56250.0);
    let f = 93.27191 + (483202.017538 * t) - (0.0036825 * t.powf(2.0)) + (t.powf(3.0) / 327270.0);
    let o = 125.04452 - (1934.136261 * t) + (0.0020708 * t.powf(2.0)) + (t.powf(3.0) / 450000.0);

    let mut pnsum;
    let mut psi = 0.0;
    let mut epsilon = 0.0;
    for i in 0..PN.len() {
        pnsum = d * COEFF[i][0] as f64;
        pnsum += m * COEFF[i][1] as f64;
        pnsum += m1 * COEFF[i][2] as f64;
        pnsum += f * COEFF[i][3] as f64;
        pnsum += o * COEFF[i][4] as f64;
        psi += (PN[i][0] as f64 + jc * PN[i][1] as f64) * deg_to_rad(pnsum).sin();
        epsilon += (PN[i][2] as f64 + jc * PN[i][3] as f64) * deg_to_rad(pnsum).cos();
    }

    let delta_psi = psi / 36_000_000.0;
    let delta_eps = epsilon / 36_000_000.0;
    let u = jm / 10.0;
    let e0 = 84381.448 - 4680.93 * u - 1.55 * u.powf(2.0) + 1999.25 * u.powf(3.0)
        - 51.38 * u.powf(4.0)
        - 249.67 * u.powf(5.0)
        - 39.05 * u.powf(6.0)
        + 7.12 * u.powf(7.0)
        + 27.87 * u.powf(8.0)
        + 5.79 * u.powf(9.0)
        + 2.45 * u.powf(10.0);
    let e = e0 / 3600.0 + delta_eps;
    let re = deg_to_rad(e);

    let lambda = g + delta_psi + (-20.4898 / (3600.0 * r));
    let rlambda = deg_to_rad(lambda);

    let v0 = 280.46061837 + 360.98564736629 * (jd - 2_451_545.0) + 0.000387933 * jc.powf(2.0)
        - jc.powf(3.0) / 38_710_000.0;
    let v = limit_angle(v0) + delta_psi * re.cos();

    let ran = rlambda.sin() * re.cos() - rgg.tan() * re.sin();
    let rad = rlambda.cos();
    let ra = limit_angle(rad_to_deg(ran.atan2(rad)));
    let dec = (rgg.sin() * re.cos() + rgg.cos() * re.sin() * rlambda.sin()).asin();

    AstroDay {
        ra,
        dec,
        sidtime: v,
        dra: 0.0,
        rsum: r,
    }
}

fn compute_top_astro(loc: &Location, astro: &Astro) -> Astro {
    let mut top = Astro::default();
    let rlat = deg_to_rad(loc.degree_lat);
    for i in 0..3 {
        let lhour = limit_angle(astro.sid[i] + loc.degree_long - astro.ra[i]);
        let rlhour = deg_to_rad(lhour);
        let sp = deg_to_rad(8.794 / (3600.0 * astro.rsum[i]));
        let tu = (0.99664719 * rlat.tan()).atan();
        let tp_sin = 0.99664719 * tu.sin() + (loc.sea_level / EARTH_RADIUS) * rlat.sin();
        let tp_cos = tu.cos() + (loc.sea_level / EARTH_RADIUS) * rlat.cos();
        let tra0 = ((-tp_cos) * sp.sin() * rlhour.sin())
            / (astro.dec[i].cos() - tp_cos * sp.sin() * rlhour.cos());
        let tra = astro.ra[i] + rad_to_deg(tra0);
        let tdec = rad_to_deg(
            ((astro.dec[i].sin() - tp_sin * sp.sin()) * tra0.cos())
                .atan2(astro.dec[i].cos() - tp_cos * sp.sin() * rlhour.cos()),
        );
        top.ra[i] = tra;
        top.dec[i] = tdec;
        top.sid[i] = astro.sid[i];
        top.dra[i] = tra0;
        top.rsum[i] = astro.rsum[i];
    }
    top.jd = astro.jd;
    top
}

pub fn get_astro_values_by_day(
    julian_day: f64,
    loc: &Location,
    astro: &mut Astro,
    top: &mut Astro,
) {
    if astro.jd == julian_day - 1.0 {
        astro.ra[0] = astro.ra[1];
        astro.ra[1] = astro.ra[2];
        astro.dec[0] = astro.dec[1];
        astro.dec[1] = astro.dec[2];
        astro.sid[0] = astro.sid[1];
        astro.sid[1] = astro.sid[2];
        astro.dra[0] = astro.dra[1];
        astro.dra[1] = astro.dra[2];
        astro.rsum[0] = astro.rsum[1];
        astro.rsum[1] = astro.rsum[2];
        let ad = compute_astro_day(julian_day + 1.0);
        astro.ra[2] = ad.ra;
        astro.dec[2] = ad.dec;
        astro.sid[2] = ad.sidtime;
        astro.dra[2] = ad.dra;
        astro.rsum[2] = ad.rsum;
    } else if astro.jd == julian_day + 1.0 {
        astro.ra[2] = astro.ra[1];
        astro.ra[1] = astro.ra[0];
        astro.dec[2] = astro.dec[1];
        astro.dec[1] = astro.dec[0];
        astro.sid[2] = astro.sid[1];
        astro.sid[1] = astro.sid[0];
        astro.dra[2] = astro.dra[1];
        astro.dra[1] = astro.dra[0];
        astro.rsum[2] = astro.rsum[1];
        astro.rsum[1] = astro.rsum[0];
        let ad = compute_astro_day(julian_day - 1.0);
        astro.ra[0] = ad.ra;
        astro.dec[0] = ad.dec;
        astro.sid[0] = ad.sidtime;
        astro.dra[0] = ad.dra;
        astro.rsum[0] = ad.rsum;
    } else if astro.jd != julian_day {
        let ad = compute_astro_day(julian_day - 1.0);
        astro.ra[0] = ad.ra;
        astro.dec[0] = ad.dec;
        astro.sid[0] = ad.sidtime;
        astro.dra[0] = ad.dra;
        astro.rsum[0] = ad.rsum;
        let ad = compute_astro_day(julian_day);
        astro.ra[1] = ad.ra;
        astro.dec[1] = ad.dec;
        astro.sid[1] = ad.sidtime;
        astro.dra[1] = ad.dra;
        astro.rsum[1] = ad.rsum;
        let ad = compute_astro_day(julian_day + 1.0);
        astro.ra[2] = ad.ra;
        astro.dec[2] = ad.dec;
        astro.sid[2] = ad.sidtime;
        astro.dra[2] = ad.dra;
        astro.rsum[2] = ad.rsum;
    }
    astro.jd = julian_day;
    *top = compute_top_astro(loc, astro);
}

pub fn get_transit(lon: f64, tastro: &Astro) -> f64 {
    let ra0 = tastro.ra[0];
    let mut ra2 = tastro.ra[2];
    let mut m = (tastro.ra[1] - lon - tastro.sid[1]) / 360.0;
    m = limit_angle_1(m);
    let sid_g = tastro.sid[1] + 360.985647 * m;
    if tastro.ra[1] > 350.0 && tastro.ra[2] < 10.0 {
        ra2 += 360.0;
    }
    let ra0 = if tastro.ra[0] > 350.0 && tastro.ra[1] < 10.0 {
        0.0
    } else {
        ra0
    };
    let a = tastro.ra[1]
        + (m * ((tastro.ra[1] - ra0)
            + (ra2 - tastro.ra[1])
            + ((ra2 - tastro.ra[1]) - (tastro.ra[1] - ra0)) * m)
            / 2.0);
    let h = limit_angle_180_between(sid_g + lon - a);
    24.0 * (m - (h / 360.0))
}

fn get_rise_set(loc: &Location, tastro: &Astro, kind: i32) -> f64 {
    let rdec = deg_to_rad(tastro.dec[1]);
    let rlat = deg_to_rad(loc.degree_lat);
    let mut ra0 = tastro.ra[0];
    let mut ra2 = tastro.ra[2];
    let part1 = deg_to_rad(CENTER_OF_SUN_ANGLE).sin() - (rlat.sin() * rdec.sin());
    let part2 = rlat.cos() * rdec.cos();
    let part3 = part1 / part2;
    if !(-INVALID_TRIGGER..=INVALID_TRIGGER).contains(&part3) {
        return 99.0;
    }
    let lhour = limit_angle_180(rad_to_deg(part3.acos()));
    let mut m = (tastro.ra[1] - loc.degree_long - tastro.sid[1]) / 360.0;
    if kind == 0 {
        m -= lhour / 360.0;
    }
    if kind == 1 {
        m += lhour / 360.0;
    }
    m = limit_angle_1(m);
    let sid_g = limit_angle(tastro.sid[1] + 360.985647 * m);
    if tastro.ra[1] > 350.0 && tastro.ra[2] < 10.0 {
        ra2 += 360.0;
    }
    if tastro.ra[0] > 350.0 && tastro.ra[1] < 10.0 {
        ra0 = 0.0;
    }
    let a = tastro.ra[1]
        + (m * ((tastro.ra[1] - ra0)
            + (ra2 - tastro.ra[1])
            + ((ra2 - tastro.ra[1]) - (tastro.ra[1] - ra0)) * m)
            / 2.0);
    let b = tastro.dec[1]
        + (m * ((tastro.dec[1] - tastro.dec[0])
            + (tastro.dec[2] - tastro.dec[1])
            + ((tastro.dec[2] - tastro.dec[1]) - (tastro.dec[1] - tastro.dec[0])) * m)
            / 2.0);
    let rb = deg_to_rad(b);
    let h = limit_angle_180_between(sid_g + loc.degree_long - a);
    let th = deg_to_rad(h) - tastro.dra[1];
    let mut sun_alt = rad_to_deg((rlat.sin() * rb.sin() + rlat.cos() * rb.cos() * th.cos()).asin());
    sun_alt += get_refraction(loc, sun_alt);
    let del_m = (sun_alt - CENTER_OF_SUN_ANGLE) / (360.0 * rb.cos() * rlat.cos() * th.sin());
    (m + del_m) * 24.0
}

pub fn get_refraction(loc: &Location, sun_alt: f64) -> f64 {
    let part1 = (loc.pressure / 1010.0) * (283.0 / (273.0 + loc.temperature));
    let part2 =
        1.02 / (rad_to_deg((deg_to_rad(sun_alt + (10.3 / (sun_alt + 5.11)))).tan()) + 0.0019279);
    (part1 * part2) / 60.0
}

pub fn get_sunrise(loc: &Location, tastro: &Astro) -> f64 {
    get_rise_set(loc, tastro, 0)
}

pub fn get_sunset(loc: &Location, tastro: &Astro) -> f64 {
    get_rise_set(loc, tastro, 1)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn julian_day_roundtrip_known() {
        let jd = get_julian_day(
            &Date {
                day: 1,
                month: 1,
                year: 2000,
            },
            0.0,
        );
        assert!((jd - 2451544.5).abs() < 0.01, "{jd}");
    }
}
