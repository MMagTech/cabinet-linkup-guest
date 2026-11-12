// <net/if.h> on Windows, for the console's rtc.cpp: the flags it reads on a link. Winsock has most of
// them; ifaddrs.h beside this never gives a link, so their values are only for the build.

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef IFF_UP
#define IFF_UP 0x1
#endif
#ifndef IFF_LOOPBACK
#define IFF_LOOPBACK 0x8
#endif
#ifndef IFF_RUNNING
#define IFF_RUNNING 0x40
#endif
