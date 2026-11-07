use prayer_core::{
    get_method, get_next_day_fajr, get_north_qibla, get_prayer_times, Date, Location,
};

fn check(
    lat: f64,
    lon: f64,
    tz: f64,
    method: i32,
    y: i32,
    m: i32,
    d: i32,
    expected: [&str; 6],
    qibla: f64,
) {
    let loc = Location {
        degree_lat: lat,
        degree_long: lon,
        gmt_diff: tz,
        dst: 0,
        ..Default::default()
    };
    let conf = get_method(method);
    let date = Date::new(y, m, d);
    let t = get_prayer_times(&loc, &conf, &date);
    let got = [
        t[0].hms(),
        t[1].hms(),
        t[2].hms(),
        t[3].hms(),
        t[4].hms(),
        t[5].hms(),
    ];
    for i in 0..6 {
        assert_eq!(
            got[i], expected[i],
            "prayer {i} for {lat},{lon} {y}-{m}-{d}"
        );
    }
    let q = get_north_qibla(&loc);
    assert!((q - qibla).abs() < 1e-4, "qibla {q} != {qibla}");
}

#[test]
fn palembang_egypt() {
    check(
        -2.9055,
        104.7612,
        7.0,
        1,
        2026,
        11,
        6,
        [
            "04:16:00", "05:36:00", "11:43:00", "15:03:00", "17:50:00", "19:01:00",
        ],
        65.473093,
    );
}

#[test]
fn palembang_isna() {
    check(
        -2.9055,
        104.7612,
        7.0,
        4,
        2026,
        11,
        6,
        [
            "04:37:00", "05:36:00", "11:43:00", "15:03:00", "17:50:00", "18:49:00",
        ],
        65.473093,
    );
}

#[test]
fn makkah_isna() {
    check(
        21.4225,
        39.8262,
        3.0,
        4,
        2026,
        6,
        21,
        [
            "04:28:00", "05:37:00", "12:21:00", "15:41:00", "19:04:00", "20:14:00",
        ],
        72.665736,
    );
}

#[test]
fn ushuaia_extreme() {
    check(
        -54.8019,
        -68.3030,
        -3.0,
        1,
        2026,
        6,
        21,
        [
            "07:26:00", "09:57:00", "13:33:00", "14:56:00", "17:10:00", "19:26:00",
        ],
        -91.691723,
    );
}

#[test]
fn london_mwl() {
    check(
        51.5074,
        -0.1278,
        0.0,
        5,
        2026,
        12,
        21,
        [
            "05:58:00", "08:01:00", "11:57:00", "13:36:00", "15:52:00", "17:49:00",
        ],
        -118.989674,
    );
}

#[test]
fn sydney_karachi() {
    check(
        -33.8688,
        151.2093,
        10.0,
        2,
        2026,
        1,
        15,
        [
            "03:17:00", "04:57:00", "12:03:00", "15:47:00", "19:07:00", "20:48:00",
        ],
        82.500994,
    );
    let loc = Location {
        degree_lat: -33.8688,
        degree_long: 151.2093,
        gmt_diff: 10.0,
        ..Default::default()
    };
    let next = get_next_day_fajr(&loc, &get_method(2), &Date::new(2026, 1, 15));
    assert_eq!(next.hms(), "03:19:00");
}
