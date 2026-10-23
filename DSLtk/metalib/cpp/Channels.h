/** Bounded/unbounded closeable FIFO channels. create 0 is unbounded.
 * create : Int -> Channel 'a; send : 'a -> Channel 'a -> Result Unit String
 * recv : Channel 'a -> Option 'a (None only when closed and drained)
 * try_send : 'a -> Channel 'a -> Bool; try_recv : Channel 'a -> Option 'a
 * recv_timeout : Int -> Channel 'a -> Option 'a (milliseconds, None on timeout)
 * close : Channel 'a -> Unit; size : Channel 'a -> Int; is_closed : Channel 'a -> Bool
 */
#pragma once
#include "Support.h"
#include <chrono>
namespace metalib { inline void install_Channels (metaml::Session &); }
#include "Channels.c"
