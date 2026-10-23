/** HTTP/HTTPS client using dynamically loaded libcurl (TLS verified).
 * type HttpResponse = { status : Int, headers : [(String, String)], body : String }
 * available : Unit -> Bool; get : String -> Result HttpResponse String
 * post : String -> String -> Result HttpResponse String
 * request : String -> String -> [(String, String)] -> String -> Int -> Result HttpResponse String
 * Arguments: method, URL, headers, bytes, timeout milliseconds (1..300000).
 * No redirects/proxies; at most 16 MiB body and 64 KiB headers.
 */
#pragma once
#include "Support.h"
#include <dlfcn.h>
namespace metalib { inline void install_HTTP (metaml::Session &); }
#include "HTTP.c"
