// <sys/socket.h> on Windows, for the console's rtc.cpp, which is built as it is written: Winsock has
// the same names (CMakeLists.txt puts this folder in front for rtc.cpp alone).

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
