/**
 * @file metalib/cpp/File.h
 * @brief MetaML host module `File` (C++ dialect) — files and paths (POSIX; errors carry the libc `strerror` text).
 *
 * Primitives (identical names, types and behaviour in metalib/d/File.d):
 *   File.read : String -> Result String String
 *   File.read_lines : String -> Result [String] String
 *   File.write : String -> String -> Result Unit String
 *   File.append : String -> String -> Result Unit String
 *   File.exists : String -> Bool
 *   File.is_dir : String -> Bool
 *   File.is_file : String -> Bool
 *   File.size : String -> Result Int String
 *   File.remove : String -> Result Unit String
 *   File.rename : String -> String -> Result Unit String
 *   File.copy : String -> String -> Result Unit String
 *   File.mkdir : String -> Result Unit String
 *   File.list_dir : String -> Result [String] String
 *   File.basename : String -> String
 *   File.dirname : String -> String
 *   File.extension : String -> String
 *   File.join : String -> String -> String
 *   File.temp_dir : Unit -> String
 *
 * Header-only: this header includes File.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace metalib
{
/// Registers the `File.*` primitives (sandboxed; see §10.4).
inline void install_File (metaml::Session &s);
} // namespace metalib

#include "File.c"
