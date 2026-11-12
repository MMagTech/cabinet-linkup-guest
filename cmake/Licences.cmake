# Every licence text a package carries, in its licenses folder, one folder for each line of Credits
# and licenses (src/screens/credits.h), under the line's own name. Included by CMakeLists.txt when it
# makes a package; twopstart_licences(<folder in the package>) installs them.
#
# Each text comes from the part's own builders, never copied by hand: GStreamer's installer keeps one
# for each part it is made of in share/licenses, vcpkg one for each port in share/<port>/copyright,
# and this repository and the submodule their own. The list is checked against credits.h each time:
# a line with no text, or a text with no line, stops the build.

set(gst_licences "${gst}/share/licenses")
set(vcpkg_share "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share")

# Name on the Credits line = a file or a folder of them. A name given twice gets both.
set(licences
    "CabinetOS 2P Start=${CMAKE_SOURCE_DIR}/LICENSE"
    "CabinetOS=${CMAKE_SOURCE_DIR}/cabinetos/LICENSE"
    "SDL=${vcpkg_share}/sdl3/copyright"
    "SDL_ttf=${vcpkg_share}/sdl3-ttf/copyright"
    "FreeType=${vcpkg_share}/freetype/copyright"
    "libpng=${vcpkg_share}/libpng/copyright"
    "bzip2=${vcpkg_share}/bzip2/copyright"
    "Brotli=${vcpkg_share}/brotli/copyright"
    "Noto Sans=${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt"
    "GStreamer=${gst_licences}/gstreamer-1.0"
    "GStreamer=${gst_licences}/gst-plugins-base-1.0"
    "GStreamer=${gst_licences}/gst-plugins-bad-1.0"
    "GLib=${gst_licences}/glib"
    "libnice=${gst_licences}/libnice"
    "libsrtp=${gst_licences}/libsrtp"
    "usrsctp=${gst_licences}/gst-plugins-bad-1.0/ext_sctp_usrsctp_LICENSE.md"
    "Opus=${gst_licences}/opus"
    "ORC=${gst_licences}/orc"
    "PCRE2=${gst_licences}/pcre2"
    "libffi=${gst_licences}/libffi"
    "proxy-libintl=${gst_licences}/proxy-libintl"
    "OpenSSL=${gst_licences}/openssl"
    "json-c=${vcpkg_share}/json-c/copyright"
    "zlib=${gst_licences}/zlib")
# Each system's own: on Windows, curl is carried, built in; on a Mac it is the system's, and the Mac's
# GStreamer decodes through MoltenVK, which Windows' does not.
if(WIN32)
    list(APPEND licences "curl=${vcpkg_share}/curl/copyright")
    set(not_carried "MoltenVK")
else()
    list(APPEND licences "MoltenVK=${gst_licences}/moltenvk")
    set(not_carried "curl")
endif()

# The names on the Credits lines.
file(STRINGS "${CMAKE_SOURCE_DIR}/src/screens/credits.h" credit_lines REGEX "^[ \t]*\\{\"[^\"]+\", \"")
set(credits "")
foreach(line IN LISTS credit_lines)
    string(REGEX REPLACE "^[ \t]*\\{\"([^\"]+)\".*" "\\1" name "${line}")
    list(APPEND credits "${name}")
endforeach()

set(named "")
foreach(entry IN LISTS licences)
    string(FIND "${entry}" "=" eq)
    string(SUBSTRING "${entry}" 0 ${eq} name)
    math(EXPR from "${eq} + 1")
    string(SUBSTRING "${entry}" ${from} -1 path)
    if(NOT name IN_LIST credits)
        message(FATAL_ERROR "A licence for ${name}, which has no line in credits.h")
    endif()
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "No licence text for ${name} at ${path}")
    endif()
    list(APPEND named "${name}")
endforeach()
foreach(name IN LISTS credits)
    if(NOT name IN_LIST named AND NOT name IN_LIST not_carried)
        message(FATAL_ERROR "credits.h names ${name}, and the package has no licence text for it")
    endif()
endforeach()

function(twopstart_licences destination)
    foreach(entry IN LISTS licences)
        string(FIND "${entry}" "=" eq)
        string(SUBSTRING "${entry}" 0 ${eq} name)
        math(EXPR from "${eq} + 1")
        string(SUBSTRING "${entry}" ${from} -1 path)
        if(IS_DIRECTORY "${path}")
            install(DIRECTORY "${path}/" DESTINATION "${destination}/${name}")
        else()
            get_filename_component(file "${path}" NAME)
            # vcpkg's are each called copyright; the folder says whose.
            if(file STREQUAL "copyright")
                set(file "LICENSE.txt")
            endif()
            install(FILES "${path}" DESTINATION "${destination}/${name}" RENAME "${file}")
        endif()
    endforeach()
    configure_file("${CMAKE_SOURCE_DIR}/cmake/licenses-readme.txt.in" "${CMAKE_BINARY_DIR}/licenses-readme.txt" @ONLY)
    install(FILES "${CMAKE_BINARY_DIR}/licenses-readme.txt" DESTINATION "${destination}" RENAME "README.txt")
endfunction()
