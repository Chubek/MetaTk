/**
 * @file metalib/cpp/Process.h
 * @brief MetaML host module `Process` (C++ dialect) — processes, environment, clocks (POSIX).
 *
 * Primitives (identical names, types and behaviour in metalib/d/Process.d):
 *   Process.run : String -> [String] -> Result (Int, String, String) String
 *   Process.shell : String -> Result (Int, String) String
 *   Process.env : String -> Option String
 *   Process.set_env : String -> String -> Unit
 *   Process.args : Unit -> [String]
 *   Process.cwd : Unit -> String
 *   Process.chdir : String -> Result Unit String
 *   Process.pid : Unit -> Int
 *   Process.clock_ms : Unit -> Int
 *   Process.time : Unit -> Float
 *   Process.exit : Int -> 'a
 *
 * Header-only: this header includes Process.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;

namespace metalib
{
/// Registers the `Process.*` primitives (sandboxed; see §10.4).
inline void install_Process (metaml::Session &s);
} // namespace metalib

#include "Process.c"
