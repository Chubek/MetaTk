// Calendar arithmetic with the date library: weekday names are 0..6.
use date;

const Y = 2026;
print day_of_week(Y, 1, 1);               // weekday of New Year
print easter(Y);                          // packed YYYYMMDD
print diff_days(pack(Y, 1, 1), pack(Y + 1, 1, 1));
let fridays = 0;
for (my m = 1; m <= 12; m++) fridays++ if day_of_week(Y, m, 13) == 5;
return fridays;                           // Friday-the-13ths this year
