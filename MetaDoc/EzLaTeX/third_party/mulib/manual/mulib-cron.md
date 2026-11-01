# MuLIB cron

**Header/switch:** `include/mulib-cron.h` / `MULIB_IMPL_CRON`  
**Dependencies:** standard C11 integer/time declarations only. C++17-linkable
declarations; no allocation, external process, clock access or threads.

## Exact supported grammar

Five fields, in order: **minute (0–59), hour (0–23), day of month (1–31), month
(1–12), day of week (0–7)**. Both 0 and 7 mean Sunday. Fields are separated by
one or more spaces/tabs; leading/trailing spaces/tabs are accepted. The entire
NUL-terminated specification is at most `MULIB_CRON_MAX_SPEC == 255` bytes.

Each field is a comma-separated nonempty list of:

* `*` — the full field range;
* `n` — one unsigned decimal number;
* `a-b` — inclusive, ascending numeric range;
* `*/step` or `a-b/step` — positive increment, starting at the range's lower bound.

Step is bounded by the field's range size (60,24,31,12,8 respectively). Leading
zeros are accepted. Ranges cannot wrap. Weekday range `1-7` means Monday through
Sunday; `7-0` is rejected. Duplicate list entries are harmless. Numeric parsing
checks range/overflow before arithmetic. `n/step` is intentionally **not** accepted.
Rejects names, macros (`@daily`), seconds/year fields, signs, empty elements,
newlines, comments, L/W/#/? extensions and timezone syntax. This is a focused
numeric subset, not a claim of compatibility with every cron implementation.

`mulib_cron_parse(&schedule, spec, optional_error_offset)` returns zero or
`MULIB_CRON_INVALID`. A failure leaves the schedule unchanged; the optional offset
identifies the parser's failure position (or length limit), not a textual error.
Schedules are value objects containing five bitsets plus day-wildcard flags.
After parsing, treat them as immutable; copying is safe. NULL arguments are
invalid. No allocation failure is possible.

## Civil-minute matching

`mulib_cron_match(&schedule, &civil_tm)` returns **1 match, 0 mismatch, -1 invalid**.
Use normal `struct tm` conventions: month is 0–11, weekday 0–6, year is years
since 1900. It checks minute/hour/month/weekday bounds and actual month length,
including Gregorian leap years; impossible February 29 dates are rejected.
Seconds, yday and isdst are ignored. Supplied weekday is used as-is and is not
recomputed or cross-validated against the date. The caller must obtain coherent
civil fields in its chosen timezone. No `mktime`, `localtime`, global timezone
state or time_t range conversion is performed.

Minute, hour and month must all match. For the two day fields:

* Literal day-of-month `*`: require day-of-week match.
* Literal day-of-week `*`: require day-of-month match.
* Both restricted: **OR** day-of-month and day-of-week.

Only an entire literal `*` field counts as unrestricted. `*/2`, `*,1` and `1-31`
are restricted syntactically, even if their resulting mask includes all values.
This precise rule avoids ambiguous dialect assumptions. With both fields literal
`*`, all valid days match. Syntactically valid but impossible combinations, e.g.
`0 0 31 2 *`, parse successfully but never match a valid civil date.

Matching is bounded, allocation-free and thread-safe for immutable schedules.
There is no daemon, next-occurrence search, sleeping, persistence or DST policy.
A repeated local minute at a daylight-saving transition matches twice if the
caller supplies it twice; a skipped local minute is never synthesized.

## Callback composition

`mulib_cron_dispatch(schedule, civil, callback, context, optional_matched)` calls
the non-NULL callback synchronously if matching. It returns zero for an unmatched
minute, the callback's original return value when matched, or INVALID for invalid
arguments. `matched` distinguishes an unmatched minute from a callback that
succeeded with zero; it is set to zero before validation. Callback errors are not
renumbered and may equal INVALID; use `matched` to disambiguate.

Dispatch does **not** deduplicate calls in the same minute. Call once per observed
minute, or track a last-fired civil/UTC minute in application state. A callback
can enqueue a task with `mulib_taskpool_submit`, handling FULL/CLOSED explicitly.
The focused test exercises exactly this composition and waits before releasing
the callback context. Cron itself does not own or depend on a pool.

## Runnable example and tests

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/cron.c -o /tmp/cron
/tmp/cron
```

The example dispatches a weekday, business-hour quarter-hour schedule. Tests cover
lists/ranges/steps, Sunday alias, both day-field semantics, malformed/extra/missing
fields, numeric and length limits, unchanged output on parse failure, leap-day
validation, invalid civil fields, unmatched dispatch and pool callback composition.
