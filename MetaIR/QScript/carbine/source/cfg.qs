// Constants shared by the Carbine engine. QScript has no enumerations, so
// every code the host exchanges with the engine is a named constant here.
// Keep them in step with tooling/internal.h (commands, options, kinds),
// tooling/natives.c (messages, status codes) and tooling/model.c.

const CMD_NONE = 0;
const CMD_BUILD = 1;
const CMD_GRAPH = 2;
const CMD_CHECK = 3;
const CMD_LINT = 4;
const CMD_GEN = 5;
const CMD_DIST = 6;
const CMD_FETCH = 7;
const CMD_VERSION = 8;
const CMD_HELP = 9;

const OPT_KEEP_GOING = 1;
const OPT_DRY_RUN = 2;
const OPT_VERBOSE = 3;

const KIND_LIB = 1;
const KIND_EXE = 2;
const KIND_PROGRAM = 3;

const SAY_BUILDING = 1;
const SAY_UP_TO_DATE = 2;
const SAY_FAILED = 3;
const SAY_SKIPPED = 4;

const MSG_CYCLE = 1;
const MSG_FAILED = 2;
const MSG_FINISHED = 3;
const MSG_VALID = 4;
const MSG_LINT = 5;

const BACKEND_NINJA = 1;
const BACKEND_MAKE = 2;
const BACKEND_CMAKE = 3;
const BACKEND_MESON = 4;
