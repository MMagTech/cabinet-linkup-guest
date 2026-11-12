// <ifaddrs.h> on Windows, for the console's rtc.cpp.
//
// rtc.cpp asks for the computer's links only to give libnice a Linux console's cable, or else its
// links on a device of their own, which it tells by files under /sys. A PC has no /sys, so every
// link would be left out (as on a Mac) and libnice would pick, as it does with no list. This gives
// no list, which comes to the same, and libnice picks.

#pragma once

#include <cerrno>

#include <winsock2.h>

struct ifaddrs {
    ifaddrs* ifa_next;
    char* ifa_name;
    unsigned int ifa_flags;
    sockaddr* ifa_addr;
    sockaddr* ifa_netmask;
    sockaddr* ifa_dstaddr;
    void* ifa_data;
};

inline int getifaddrs(ifaddrs** out) {
    *out = nullptr;
    errno = ENOSYS;
    return -1;
}

inline void freeifaddrs(ifaddrs*) {}
