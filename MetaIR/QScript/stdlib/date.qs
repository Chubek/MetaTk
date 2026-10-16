// date: proleptic Gregorian calendar arithmetic.
//
//     use date;
//     let d = days_from_civil(2024, 2, 29);        // days since 1970-01-01
//     print day_of_week(2024, 2, 29), civil(d + 1), easter(2025);
//
// Dates are passed as (year, month, day) with months 1..12, or packed as
// the integer YYYYMMDD (20240301) by civil(), pack() and add_days().
// Weekdays are 0 = Sunday .. 6 = Saturday.
fn is_leap(y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
fn days_in_year(y) { return is_leap(y) ? 366 : 365; }
fn days_in_month(y, m) {
    assert m >= 1 && m <= 12;
    if (m == 2) return is_leap(y) ? 29 : 28;
    if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
    return 31;
}
fn is_valid(y, m, d) { return m >= 1 && m <= 12 && d >= 1 && d <= days_in_month(y, m); }
// Days since 1970-01-01 (negative before), after Howard Hinnant.
fn days_from_civil(y, m, d) {
    if (m <= 2) y--;
    let era = (y >= 0 ? y : y - 399) / 400;
    let yoe = y - era * 400;
    let doy = (153 * ((m + 9) % 12) + 2) / 5 + d - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
// The date `days` after 1970-01-01, packed as YYYYMMDD (years >= 0).
fn civil(days) {
    let z = days + 719468;
    let era = (z >= 0 ? z : z - 146096) / 146097;
    let doe = z - era * 146097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = mp < 10 ? mp + 3 : mp - 9;
    let y = yoe + era * 400 + (m <= 2);
    return y * 10000 + m * 100 + d;
}
fn pack(y, m, d) { return y * 10000 + m * 100 + d; }
fn year(p) { return p / 10000; }
fn month(p) { return p / 100 % 100; }
fn day(p) { return p % 100; }
fn to_days(p) { return days_from_civil(year(p), month(p), day(p)); }
fn add_days(p, n) { return civil(to_days(p) + n); }
fn diff_days(p1, p2) { return to_days(p2) - to_days(p1); }
fn day_of_week(y, m, d) {
    let w = (days_from_civil(y, m, d) + 4) % 7;     // 1970-01-01 was a Thursday
    return w < 0 ? w + 7 : w;
}
fn day_of_year(y, m, d) { return days_from_civil(y, m, d) - days_from_civil(y, 1, 1) + 1; }
// Western (Gregorian) Easter Sunday, packed (anonymous Gregorian algorithm).
fn easter(y) {
    let a = y % 19;
    let b = y / 100;
    let c = y % 100;
    let d = b / 4;
    let e = b % 4;
    let f = (b + 8) / 25;
    let g = (b - f + 1) / 3;
    let h = (19 * a + b - d - g + 15) % 30;
    let i = c / 4;
    let k = c % 4;
    let l = (32 + 2 * e + 2 * i - h - k) % 7;
    let m = (a + 11 * h + 22 * l) / 451;
    let month = (h + l - 7 * m + 114) / 31;
    let day = (h + l - 7 * m + 114) % 31 + 1;
    return y * 10000 + month * 100 + day;
}
