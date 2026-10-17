// time: seconds, clocks and Unix timestamps.
//
//     use time;
//     let t = unix(2000, 1, 1, 12, 30, 0);
//     print t, clock(time_of_day(t)), weekday(t);   // 946729800 123000 6
//
// Timestamps are seconds since 1970-01-01 00:00:00 UTC; clocks pack a time
// of day as HHMMSS.
use date;

const MINUTE = 60;
const HOUR = 3600;
const DAY = 86400;
const WEEK = 604800;

fn hms(h, m, s) { return h * HOUR + m * MINUTE + s; }
fn hours(secs) { return secs / HOUR; }
fn minutes(secs) { return secs / MINUTE % 60; }
fn seconds(secs) { return secs % 60; }
fn clock(secs) { return hours(secs) * 10000 + minutes(secs) * 100 + seconds(secs); }
fn unix(y, mo, d, h, mi, s) { return days_from_civil(y, mo, d) * DAY + hms(h, mi, s); }
// Day number and second of the day, rounding toward negative infinity.
fn unix_days(t) { let d = t / DAY; return (t % DAY < 0) ? d - 1 : d; }
fn time_of_day(t) { let r = t % DAY; return r < 0 ? r + DAY : r; }
fn unix_date(t) { return civil(unix_days(t)); }
fn weekday(t) { let w = (unix_days(t) + 4) % 7; return w < 0 ? w + 7 : w; }
fn hour12(h) { return h % 12 == 0 ? 12 : h % 12; }
fn is_pm(h) { return h >= 12; }
fn to_ms(s) { return s * 1000; }
fn from_ms(ms) { return ms / 1000; }
