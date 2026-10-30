#ifndef METAVM_TERSE_SYNTAX_H
#define METAVM_TERSE_SYNTAX_H
#include "Compiler.h"
/* Line-oriented grammar (ASCII identifiers, decimal integers):
 * module   := (blank | import | function)*
 * import   := '.import' name arity newline
 * function := '.function' name arity registers newline (label | insn)* '.end'
 * label    := name ':' newline
 * insn     := opcode operands newline
 * register := 'r' unsigned-decimal
 * Comments start with '#' or ';'. Commas are optional operand separators.
 * jmp/jz take labels; call/host take names (forward references allowed).
 * Other operands follow AbstractMachine.h; instructions each occupy one line.
 * Names: [A-Za-z_][A-Za-z0-9_]*, at most 255 characters. */
#endif
