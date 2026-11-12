# The libraries the Windows zip needs, at install time (CMakeLists.txt): those the app and its
# GStreamer plugins name, and those those name, all the way down, each from GStreamer's own folder.
# What Windows itself has stays out. Anything found elsewhere, or not found, stops the install, so
# the zip never carries a stray copy from whatever else is on the computer that built it.
#
# Paths are read with / between their parts, whichever way Windows writes them.
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

# Given by CMakeLists.txt: TWOPSTART_EXE, TWOPSTART_GST_PLUGINS and TWOPSTART_GST_BIN, and dumpbin
# as the tool that reads each library's list.

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${TWOPSTART_EXE}"
    MODULES ${TWOPSTART_GST_PLUGINS}
    DIRECTORIES "${TWOPSTART_GST_BIN}"
    PRE_EXCLUDE_REGEXES "^[Aa][Pp][Ii]-[Mm][Ss]-" "^[Ee][Xx][Tt]-[Mm][Ss]-"
    POST_EXCLUDE_REGEXES "^[A-Za-z]:.[Ww][Ii][Nn][Dd][Oo][Ww][Ss]."
    RESOLVED_DEPENDENCIES_VAR found
    UNRESOLVED_DEPENDENCIES_VAR missing)

if(missing)
    message(FATAL_ERROR "Libraries the zip needs that were not found: ${missing}")
endif()

string(TOLOWER "${TWOPSTART_GST_BIN}/" gst_bin)
set(stray "")
foreach(library IN LISTS found)
    file(TO_CMAKE_PATH "${library}" path)
    string(TOLOWER "${path}" path)
    string(FIND "${path}" "${gst_bin}" at)
    if(NOT at EQUAL 0)
        list(APPEND stray "${library}")
    endif()
endforeach()
if(stray)
    message(FATAL_ERROR "Libraries found outside GStreamer's folder, which the zip will not carry: ${stray}")
endif()

list(LENGTH found count)
message(STATUS "GStreamer's libraries in the zip: ${count}")
file(INSTALL ${found} DESTINATION "${CMAKE_INSTALL_PREFIX}")
