/** Stackful, thread-affine cooperative fibers (POSIX ucontext).
 * create : (Unit -> 'a) -> Fiber 'a
 * resume : Fiber 'a -> Result (Option 'a) String (None means yielded)
 * yield : Unit -> Unit; cancel : Fiber 'a -> Unit; status : Fiber 'a -> String
 * Completed results are cached. Cancellation unwinds a suspended fiber.
 */
#pragma once
#include "Support.h"
#include <ucontext.h>
namespace metalib { inline void install_Fiber (metaml::Session &); }
#include "Fiber.c"
