// units: unit conversions in scaled integers.
//
//     use units;
//     print c_to_f(37000), miles_to_m(26), kib(64), deg_to_mrad(180);   // 98600 41842 65536 3141
//
// Temperatures are in thousandths of a degree (37000 = 37.000 C), lengths
// in metres or millimetres, masses in grams, angles in degrees or
// milliradians, data sizes in bytes. Conversions truncate toward zero.
const KB = 1000;
const MB = 1000000;
const GB = 1000000000;
const KIB = 1024;
const MIB = 1048576;
const GIB = 1073741824;

fn c_to_f(mc) { return mc * 9 / 5 + 32000; }
fn f_to_c(mf) { return (mf - 32000) * 5 / 9; }
fn c_to_k(mc) { return mc + 273150; }
fn k_to_c(mk) { return mk - 273150; }
fn inches_to_mm(i) { return i * 254 / 10; }
fn feet_to_mm(f) { return f * 3048 / 10; }
fn mm_to_inches(mm) { return mm * 10 / 254; }
fn miles_to_m(mi) { return mi * 1609344 / 1000; }
fn m_to_miles(m) { return m * 1000 / 1609344; }
fn km_to_m(km) { return km * 1000; }
fn pounds_to_g(lb) { return lb * 45359237 / 100000; }
fn g_to_pounds(g) { return g * 100000 / 45359237; }
fn ounces_to_g(oz) { return oz * 28349523 / 1000000; }
fn deg_to_mrad(d) { return d * 3141593 / 180000; }
fn mrad_to_deg(mr) { return mr * 180000 / 3141593; }
fn kib(n) { return n * KIB; }
fn mib(n) { return n * MIB; }
fn gib(n) { return n * GIB; }
// Bytes to whole KiB / MiB, rounding up.
fn to_kib(bytes) { return (bytes + KIB - 1) / KIB; }
fn to_mib(bytes) { return (bytes + MIB - 1) / MIB; }
