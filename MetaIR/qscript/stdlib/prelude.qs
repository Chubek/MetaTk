// prelude: shared integer constants.
//
//     use prelude;
//     return INT_MAX;
//
// Constants cost nothing at run time: they are folded into the code that
// uses them and occupy none of the 16 variable registers.
const TRUE = 1;
const FALSE = 0;
const INT_MAX = 9223372036854775807;
const INT_MIN = -9223372036854775808;
const NEG_ONE = -1;
const MASK8 = 0xff;
const MASK16 = 0xffff;
const MASK32 = 0xffffffff;
