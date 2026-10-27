# 10 — Groups and delayed tokens

Braces in executed input create groups. Macro definitions, group-local C++
handlers, integer-register assignments, category changes, and inline style
changes are restored when a group ends. `\gdef`, `\xdef`, and supported
`\global` assignments deliberately survive that restoration.

<!-- metatex-example {"format":"plain","text":"B A"} -->
```tex
\def\name{A}
{\def\name{B}\name}
\name
```

Argument braces have a different role: the argument scanner removes a single
outer pair without executing it. The receiving command decides whether and
how to execute the contained tokens. Do not assume every braced argument
creates a runtime assignment scope.

`\aftergroup` reads one unexpanded token and queues it for insertion after the
current group closes. Multiple queued tokens retain their order. They execute
after local state has been restored:

<!-- metatex-example {"format":"plain","text":"BA"} -->
```tex
\def\name{A}
{\def\name{B}\aftergroup\name\name}
```

At outermost level, `\aftergroup` consumes its token without scheduling it.
An unfinished group is diagnosed; the next compilation discards unfinished
local scopes. This engine does not yet model every distinct TeX group type,
and plugin-maintained named counters are separate from scoped integer registers.

[Contents](README.md)
