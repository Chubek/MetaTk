#pragma once
#include "GPPX.h"
#include <string>
#include <string_view>
#include <vector>
namespace webber {
/** Two-pass assembler for the documented Webber example stack VM.
 * Throws std::runtime_error with a source-line position on parse/encode error.
 * Labels, forward references and .EQU constants are resolved in pass two.
 * Maximum assembled program size is 65,536 bytes.
 */
std::vector<unsigned char> assemble(std::string_view source);
/** Encode binary bytecode as comma-separated C hex integer literals. */
std::string asCArray(const std::vector<unsigned char>& bytes);
}
