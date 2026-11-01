# mulib-keyremap

Stateful, one-event-at-a-time key-code mapping over `mulib-input`. No textual
configuration language, modifiers, chords, macros, key names or device injection.

## Integration and lifetime

Define `MULIB_IMPL_INPUT` and `MULIB_IMPL_KEYREMAP` in one C11 translation unit.
`mulib_keyremap_init(&remap)` establishes identity mappings and clears held state.
State is fixed-size, value-owned, allocation-free (roughly 5.4 KiB). No cleanup
is required. Do not reset or discard held state before emitting releases to a
downstream virtual device. State is independent per instance; no global mappings.

`set(&remap,source,target)` accepts codes 0..767 or target
`MULIB_KEYREMAP_DROP`. It returns 1 on success, 0 on invalid arguments. Changes
take effect at the next press, not on already-held keys.

## Event semantics

`apply(&remap,&event,&out)` returns 1 for an output event, 0 for suppression, -1
for invalid input without mutation. Output is untouched on 0/-1 and may alias
input. Non-key events pass through. Presses capture the current target; repeats
and releases continue to use that target even after mapping changes. Duplicate
presses and orphan releases/repeats are suppressed.

Many source keys may target one key: only the first press and last release are
emitted. Held-key repeats still pass through. Dropped keys produce no events.
SYN_REPORT passes through even for an empty mapped frame. SYN_DROPPED is not
automatically recoverable: stop forwarding, reconcile native held state and
release/reset downstream state before resuming. No callback failures to roll back.

`cc -std=c11 -Iinclude examples/modules/keyremap.c -o keyremap`.
`tests/test_devices.c` checks many-to-one balancing, remapping while held,
suppression, aliasing, drops and malformed events.
