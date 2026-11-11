# CabinetOS 2P Start. The one build, used by CI (.github/workflows/build.yml) and by hand:
#
#   cmake --preset default && cmake --build --preset default && ctest --preset default
#
# The console's own LinkUp code is built straight from cabinetos/, the CabinetOS submodule, and is
# never edited or copied by hand. What Windows lacks is filled in from src/win/.

cmake_minimum_required(VERSION 3.25)
project(cabinet-linkup-guest VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LINKUP "${CMAKE_SOURCE_DIR}/cabinetos/linkup")
set(FRONTEND "${CMAKE_SOURCE_DIR}/cabinetos/frontend/src")
if(NOT EXISTS "${LINKUP}/noise.cpp")
    message(FATAL_ERROR "cabinetos/ is empty. Run: git submodule update --init")
endif()

# OpenSSL 3.2 or later: Argon2id, the slow hash of a code, came in with 3.2. On a Mac it is
# Homebrew's, which CMake does not find by itself. On Windows it is GStreamer's own, so that the
# program holds one OpenSSL: OPENSSL_ROOT_DIR is given on the command line.
if(APPLE AND NOT OPENSSL_ROOT_DIR)
    execute_process(COMMAND brew --prefix openssl@3
                    OUTPUT_VARIABLE brew_openssl OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(brew_openssl)
        set(OPENSSL_ROOT_DIR "${brew_openssl}")
    endif()
endif()
find_package(OpenSSL 3.2 REQUIRED COMPONENTS Crypto)
find_package(CURL REQUIRED)
find_package(json-c CONFIG REQUIRED)
find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/W3 /utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

# The two headers LinkUp borrows from the console's main program, and only those two: the rest of
# that folder is the console's, and its names must not answer an #include here.
set(BORROWED "${CMAKE_BINARY_DIR}/borrowed")
configure_file("${FRONTEND}/fileio.h" "${BORROWED}/fileio.h" COPYONLY)
configure_file("${FRONTEND}/linkup_wire.h" "${BORROWED}/linkup_wire.h" COPYONLY)

# What a console says to a LinkUp server: Noise, pairing, sessions, sealed boxes, and what it keeps.
add_library(linkup_protocol STATIC
    "${LINKUP}/noise.cpp"
    "${LINKUP}/box.cpp"
    "${LINKUP}/keys.cpp"
    "${LINKUP}/server_link.cpp"
    "${LINKUP}/pairing.cpp")
target_include_directories(linkup_protocol PUBLIC "${LINKUP}" "${BORROWED}")
target_link_libraries(linkup_protocol PUBLIC OpenSSL::Crypto CURL::libcurl json-c::json-c Threads::Threads)
if(WIN32)
    # fileio.h done the Windows way, and the few POSIX names the code uses, in every file.
    target_sources(linkup_protocol PRIVATE src/win/fileio_win.cpp)
    target_compile_definitions(linkup_protocol PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(linkup_protocol PUBLIC "/FI${CMAKE_SOURCE_DIR}/src/win/compat.h")
    else()
        target_compile_options(linkup_protocol PUBLIC -include "${CMAKE_SOURCE_DIR}/src/win/compat.h")
    endif()
    target_link_libraries(linkup_protocol PUBLIC shell32 ole32)
else()
    target_sources(linkup_protocol PRIVATE "${FRONTEND}/fileio.cpp")
endif()

# 2P Start's own: who this friend is to a server, and where the app keeps its folder.
add_library(guest STATIC
    src/guest/account.cpp
    src/guest/paths.cpp)
target_include_directories(guest PUBLIC src)
target_link_libraries(guest PUBLIC linkup_protocol)

add_executable(2pstart-cli src/cli/main.cpp)
target_link_libraries(2pstart-cli PRIVATE guest)

# CabinetOS's own console on the command line, for trying things side by side. Its fake server is
# written on POSIX sockets, so it is not built on Windows.
if(NOT WIN32)
    add_executable(linkup-probe "${LINKUP}/probe.cpp")
    target_link_libraries(linkup-probe PRIVATE linkup_protocol)
endif()

enable_testing()

# CabinetOS's Noise test, against the published vectors and the server's own answers, which sit
# beside it in the submodule. Its ninth part does not build here yet (cmake/NoiseTestWithoutSound.cmake).
include(cmake/NoiseTestWithoutSound.cmake)
noise_test_without_sound("${LINKUP}/noise-test.cpp" "${CMAKE_BINARY_DIR}/generated/noise-test.cpp")
add_executable(noise-test "${CMAKE_BINARY_DIR}/generated/noise-test.cpp")
target_link_libraries(noise-test PRIVATE linkup_protocol)
add_test(NAME noise-test COMMAND noise-test "${LINKUP}")

add_executable(guest-test tests/guest_test.cpp)
target_link_libraries(guest-test PRIVATE guest)
add_test(NAME guest-test COMMAND guest-test)
