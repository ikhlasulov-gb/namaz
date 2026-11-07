use prayer_core::{
    get_method, get_next_day_fajr, get_north_qibla, get_prayer_times, Date, Location,
};

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let lat = args.get(1).and_then(|s| s.parse().ok()).unwrap_or(-2.9055);
    let lon = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(104.7612);
    let tz = args.get(3).and_then(|s| s.parse().ok()).unwrap_or(7.0);
    let method = args.get(4).and_then(|s| s.parse().ok()).unwrap_or(1);
    let year = args.get(5).and_then(|s| s.parse().ok()).unwrap_or(2026);
    let month = args.get(6).and_then(|s| s.parse().ok()).unwrap_or(11);
    let day = args.get(7).and_then(|s| s.parse().ok()).unwrap_or(6);

    let loc = Location {
        degree_lat: lat,
        degree_long: lon,
        gmt_diff: tz,
        dst: 0,
        ..Default::default()
    };
    let conf = get_method(method);
    let date = Date::new(year, month, day);
    let times = get_prayer_times(&loc, &conf, &date);
    for (i, name) in ["Fajr", "Shuruq", "Dhuhr", "Asr", "Maghrib", "Ishaa"]
        .iter()
        .enumerate()
    {
        println!("{name:8} {}", times[i].hms());
    }
    let nf = get_next_day_fajr(&loc, &conf, &date);
    println!("NextFajr {}", nf.hms());
    println!("Qibla    {:.6}", get_north_qibla(&loc));
}
