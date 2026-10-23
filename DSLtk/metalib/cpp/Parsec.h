/**
 * @file metalib/cpp/Parsec.h
 * @brief MetaML host module `Parsec` (C++ dialect) — parser combinators as host objects (`Parser 'a`).
 *
 * Primitives (identical names, types and behaviour in metalib/d/Parsec.d):
 *   type Parser 'a   (opaque host handle)
 *   Parsec.char : String -> Parser String
 *   Parsec.string : String -> Parser String
 *   Parsec.any : Unit -> Parser String
 *   Parsec.satisfy : (String -> Bool) -> Parser String
 *   Parsec.one_of : String -> Parser String
 *   Parsec.seq : Parser 'a -> Parser 'b -> Parser ('a, 'b)
 *   Parsec.left : Parser 'a -> Parser 'b -> Parser 'a
 *   Parsec.right : Parser 'a -> Parser 'b -> Parser 'b
 *   Parsec.between : Parser 'o -> Parser 'c -> Parser 'a -> Parser 'a
 *   Parsec.alt : Parser 'a -> Parser 'a -> Parser 'a
 *   Parsec.choice : [Parser 'a] -> Parser 'a
 *   Parsec.many : Parser 'a -> Parser ['a]
 *   Parsec.many1 : Parser 'a -> Parser ['a]
 *   Parsec.sep_by : Parser 'a -> Parser 's -> Parser ['a]
 *   Parsec.map : ('a -> 'b) -> Parser 'a -> Parser 'b
 *   Parsec.bind : Parser 'a -> ('a -> Parser 'b) -> Parser 'b
 *   Parsec.pure : 'a -> Parser 'a
 *   Parsec.fail : String -> Parser 'a
 *   Parsec.optional : Parser 'a -> Parser (Option 'a)
 *   Parsec.lazy : (Unit -> Parser 'a) -> Parser 'a
 *   Parsec.label : String -> Parser 'a -> Parser 'a
 *   Parsec.regex : String -> Parser String
 *   Parsec.token : Parser 'a -> Parser 'a
 *   Parsec.spaces : Unit -> Parser Unit
 *   Parsec.eof : Unit -> Parser Unit
 *   Parsec.run : Parser 'a -> String -> Result 'a String
 *   Parsec.run_prefix : Parser 'a -> String -> Result ('a, String) String
 *
 *   Parsers backtrack on failure; errors report the furthest position reached.
 *
 * Header-only: this header includes Parsec.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include "FlexBridge.h"

namespace metalib
{
/// Registers the `Parsec.*` primitives (sandboxed; see §10.4).
inline void install_Parsec (metaml::Session &s);
} // namespace metalib

#include "Parsec.c"
