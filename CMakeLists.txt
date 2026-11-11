# CabinetOS 2P Start. The one build, used by CI (.github/workflows/build.yml) and by hand:
#
#   cmake --preset default && cmake --build --preset default && ctest --preset default
#
# The console's own LinkUp code is built straight from cabinetos/, the CabinetOS submodule, and is
# never edited or copied by hand. What Windows lacks is filled in from src/win/, and the one piece of
# PipeWire the console's sound queue uses from src/pipewire/.

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

# OpenSSL 3.2 or later: Argon2id, the slow hash of a code, came in with 3.2.
#
# On Windows it is GStreamer's own, so that the program holds one OpenSSL once the stream is built
# on it: -DGSTREAMER_ROOT=<where GStreamer is installed>. GStreamer names its import library
# lib/crypto.lib, which CMake's own search does not expect, so the target is made here.
# On a Mac it is Homebrew's, which CMake does not find by itself.
#
# GStreamer itself is GStreamer's own build, from its installer, on every system: the same version
# from the same place, held to its checksum in .github/workflows/build.yml.
if(APPLE)
    set(gst_default "/Library/Frameworks/GStreamer.framework/Versions/1.0")
else()
    set(gst_default "")
endif()
set(GSTREAMER_ROOT "${gst_default}" CACHE PATH "Where GStreamer is installed")
if(WIN32 AND GSTREAMER_ROOT)
    file(TO_CMAKE_PATH "${GSTREAMER_ROOT}" gst)
    if(NOT EXISTS "${gst}/lib/crypto.lib" OR NOT EXISTS "${gst}/include/openssl/opensslv.h")
        message(FATAL_ERROR "No OpenSSL in GStreamer at ${gst}: lib/crypto.lib and include/openssl are wanted.")
    endif()
    file(STRINGS "${gst}/include/openssl/opensslv.h" openssl_version
         REGEX "^#[ \t]*define[ \t]+OPENSSL_VERSION_(MAJOR|MINOR)[ \t]+[0-9]+")
    string(REGEX REPLACE ".*OPENSSL_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" openssl_major "${openssl_version}")
    string(REGEX REPLACE ".*OPENSSL_VERSION_MINOR[ \t]+([0-9]+).*" "\\1" openssl_minor "${openssl_version}")
    message(STATUS "GStreamer's OpenSSL: ${openssl_major}.${openssl_minor}")
    if("${openssl_major}.${openssl_minor}" VERSION_LESS 3.2)
        message(FATAL_ERROR "GStreamer's OpenSSL is ${openssl_major}.${openssl_minor}. Argon2id needs 3.2 or later.")
    endif()
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${gst}/lib/crypto.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${gst}/include")
else()
    if(APPLE AND NOT OPENSSL_ROOT_DIR)
        execute_process(COMMAND brew --prefix openssl@3
                        OUTPUT_VARIABLE brew_openssl OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(brew_openssl)
            set(OPENSSL_ROOT_DIR "${brew_openssl}")
        endif()
    endif()
    find_package(OpenSSL 3.2 REQUIRED COMPONENTS Crypto)
endif()
find_package(CURL REQUIRED)
find_package(json-c CONFIG REQUIRED)
find_package(Threads REQUIRED)
# The window, the controllers and the words on every screen.
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

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
configure_file("${FRONTEND}/linkuprules.h" "${BORROWED}/linkuprules.h" COPYONLY)

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

# 2P Start's own: who this friend is to a server, where the app keeps its folder and its settings,
# and the friend's side of the server.
add_library(guest STATIC
    src/guest/account.cpp
    src/guest/link.cpp
    src/guest/paths.cpp
    src/guest/settings.cpp)
target_include_directories(guest PUBLIC src)
target_link_libraries(guest PUBLIC linkup_protocol)

# The game: its connection, its picture and its sound (src/game), and the console's own connection
# code, rtc.cpp, as it is written. Built on a Mac for now. Windows plays nothing until its picture and
# sound are built: src/game/none.cpp stands in there, and Join ends the session at once. The same
# stand-in builds anywhere with -DTWOPSTART_GAME=OFF, for working on the rest with no GStreamer.
option(TWOPSTART_GAME "Build the game's picture, sound and connection" ${APPLE})
#
# The console's sound queue is cut out of friend_sound.cpp, where it sits beside PipeWire, into a file
# of its own (cmake/SoundQueue.cmake). It is built on every system, for the Noise test.
include(cmake/SoundQueue.cmake)
set(GENERATED "${CMAKE_BINARY_DIR}/generated")
sound_queue("${LINKUP}/friend_sound.h" "${LINKUP}/friend_sound.cpp" "${GENERATED}")
add_library(sound_queue STATIC "${GENERATED}/sound_queue.cpp")
target_include_directories(sound_queue PUBLIC "${GENERATED}" "${CMAKE_SOURCE_DIR}/src/pipewire")

if(TWOPSTART_GAME)
    # GStreamer's own pkg-config, and its files, which say where everything is inside its framework.
    # Here only: what was found before this, for curl, keeps its own.
    find_package(PkgConfig REQUIRED)
    if(EXISTS "${GSTREAMER_ROOT}/bin/pkg-config")
        set(PKG_CONFIG_EXECUTABLE "${GSTREAMER_ROOT}/bin/pkg-config")
    endif()
    if(NOT EXISTS "${GSTREAMER_ROOT}/lib/pkgconfig/gstreamer-webrtc-1.0.pc")
        message(FATAL_ERROR "No GStreamer at ${GSTREAMER_ROOT}. Install GStreamer's own runtime and development "
                            "packages for macOS (the version in .github/workflows/build.yml), or set -DGSTREAMER_ROOT.")
    endif()
    set(ENV{PKG_CONFIG_PATH} "${GSTREAMER_ROOT}/lib/pkgconfig")
    pkg_check_modules(GST REQUIRED IMPORTED_TARGET
        gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0)
    add_library(game STATIC src/game/game.cpp src/game/stream.cpp src/game/sound.cpp "${LINKUP}/rtc.cpp")
    target_link_libraries(game PUBLIC sound_queue PkgConfig::GST)
    # As the console's linkup/Makefile: webrtcbin's library says it may still change.
    target_compile_definitions(game PUBLIC GST_USE_UNSTABLE_API)
else()
    add_library(game STATIC src/game/none.cpp)
endif()
target_include_directories(game PUBLIC src "${BORROWED}")
target_link_libraries(game PUBLIC linkup_protocol SDL3::SDL3)

add_executable(2pstart-cli src/cli/main.cpp)
target_link_libraries(2pstart-cli PRIVATE guest)

# CabinetOS's own console on the command line, for trying things side by side. Its fake server is
# written on POSIX sockets, so it is not built on Windows.
if(NOT WIN32)
    add_executable(linkup-probe "${LINKUP}/probe.cpp")
    target_link_libraries(linkup-probe PRIVATE linkup_protocol)
endif()

# The screens: how to draw (src/ui), what each shows (src/screens), and LinkUp's rules and words,
# which are the console's own file, so the app says what the console says.
add_library(screens STATIC
    src/ui/draw.cpp
    src/ui/look.cpp
    src/ui/parts.cpp
    src/screens/screens.cpp
    "${FRONTEND}/linkuprules.cpp")
target_include_directories(screens PUBLIC src "${BORROWED}")
# The app's version, for About and the window: the project's, above.
target_compile_definitions(screens PUBLIC TWOPSTART_VERSION="${PROJECT_VERSION}")
target_link_libraries(screens PUBLIC SDL3::SDL3
    $<IF:$<TARGET_EXISTS:SDL3_ttf::SDL3_ttf>,SDL3_ttf::SDL3_ttf,SDL3_ttf::SDL3_ttf-static>)

# Every screen with made-up friends, in a window or as pictures. The fonts go beside it.
add_executable(2pstart-gallery src/gallery/main.cpp)
target_link_libraries(2pstart-gallery PRIVATE screens)
add_custom_command(TARGET 2pstart-gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:2pstart-gallery>/assets")

# The app's parts with no window: where focus goes, what the hands ask for, and what a line keeps.
add_library(app_core STATIC
    src/app/input.cpp
    src/app/moves.cpp
    src/app/textline.cpp)
target_link_libraries(app_core PUBLIC guest screens)

# Where the app meets the computer, each system's own way: whether anybody is at it (idle), opening at
# login (startup), and the tray, the Dock and notifications (tray).
add_library(app_system STATIC src/app/idle.cpp src/app/startup.cpp src/app/tray.cpp)
target_include_directories(app_system PUBLIC src)
target_link_libraries(app_system PUBLIC SDL3::SDL3)
if(APPLE)
    enable_language(OBJCXX)
    target_sources(app_system PRIVATE src/app/tray_mac.mm)
    set_source_files_properties(src/app/tray_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(app_system PUBLIC "-framework AppKit" "-framework ApplicationServices" "-framework IOKit"
                                            "-framework UserNotifications")
elseif(WIN32)
    target_sources(app_system PRIVATE src/app/tray_win.cpp)
    target_link_libraries(app_system PUBLIC shell32 user32 wtsapi32 powrprof)
else()
    target_sources(app_system PRIVATE src/app/tray_none.cpp)
endif()
target_link_libraries(app_core PUBLIC app_system)

# The app: its window, and what each press does. On Windows it opens with no console window behind
# it. The fonts go beside it.
add_executable(2pstart WIN32 src/app/main.cpp src/app/app.cpp src/app/session.cpp src/app/trying.cpp)
target_link_libraries(2pstart PRIVATE app_core game)
add_custom_command(TARGET 2pstart POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:2pstart>/assets")

enable_testing()

# CabinetOS's Noise test, against the published vectors and the server's own answers, which sit
# beside it in the submodule. All of it: its ninth part holds the sound queue to a host's clock.
noise_test_with_queue("${LINKUP}/noise-test.cpp" "${GENERATED}/noise-test.cpp")
add_executable(noise-test "${GENERATED}/noise-test.cpp")
target_link_libraries(noise-test PRIVATE linkup_protocol sound_queue)
add_test(NAME noise-test COMMAND noise-test "${LINKUP}")

add_executable(guest-test tests/guest_test.cpp)
target_link_libraries(guest-test PRIVATE guest)
add_test(NAME guest-test COMMAND guest-test)

add_executable(app-test tests/app_test.cpp)
target_link_libraries(app-test PRIVATE app_core)
add_test(NAME app-test COMMAND app-test)

# Every screen drawn once into memory and saved: a screen that cannot be drawn fails here.
add_test(NAME gallery-shots COMMAND 2pstart-gallery --shots "${CMAKE_BINARY_DIR}/shots")
