# ArchLD hardening baseline (2026-10-24)

Before source changes, `make -j4 test` passed both existing Python suites:
real ELF linking/execution and DSLtk tooling. No compiler warnings were emitted.
The suites did not report a check count.

An independent Clang ASan/UBSan rebuild also passed both suites without
sanitizer findings (3.777 seconds including compilation):

```sh
make -B -j4 test CC=clang CXX=clang++ \
  CFLAGS='-O1 -g -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  CXXFLAGS='-O1 -g -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined'
```

Dependency inspection showed that the obsolete `-I../dsltk` silently fell
back to `/usr/local/include/DSLtk.hpp`. The workspace toolkit is at
`../../DSLtk`. No file or reference named ArchQFD was found in the workspace;
the actual object reader used here is the standalone ArchBFD copy.
The user subsequently confirmed ArchBFD as the intended dependency.
