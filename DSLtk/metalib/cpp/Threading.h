/** Managed worker tasks. Session shutdown cancels then joins every worker.
 * spawn : (Unit -> 'a) -> Thread 'a; join : Thread 'a -> Result 'a String
 * cancel : Thread 'a -> Unit; finished : Thread 'a -> Bool
 * sleep : Int -> Unit (milliseconds); yield : Unit -> Unit
 * Worker permissions are snapshotted; Loader/Module and nested spawn/create
 * are denied. A Session must outlive its workers. No detached threads.
 */
#pragma once
#include "Support.h"
#include <chrono>
namespace metalib { inline void install_Threading (metaml::Session &); }
#include "Threading.c"
