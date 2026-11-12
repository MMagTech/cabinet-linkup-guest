# The libraries the Mac app needs, at install time (CMakeLists.txt, "The Mac app"): those the app and
# its GStreamer plugins name, and those those name, all the way down, each from GStreamer's own
# folder, into the app's Frameworks. What macOS itself has stays out. Anything found elsewhere, or
# not found, stops the install, so the app never carries a stray copy from whatever else is on the
# computer that built it.
#
# Each is then cut to Apple silicon's half (GStreamer's are for both kinds of Mac), told to look for
# the others inside the app and nowhere else, and signed for this computer alone, and then the app
# as a whole, which is all a Mac asks to run it. It is not signed for other computers, so a friend's
# Mac asks before it first opens it.
#
# Given by CMakeLists.txt: TWOPSTART_EXE (the app, already in the bundle), TWOPSTART_GST_PLUGINS
# (GStreamer's own, whose copies are in the bundle's Library/gstreamer-1.0 too) and TWOPSTART_GST_LIB,
# GStreamer's lib folder.

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${TWOPSTART_EXE}"
    MODULES ${TWOPSTART_GST_PLUGINS}
    DIRECTORIES "${TWOPSTART_GST_LIB}"
    POST_EXCLUDE_REGEXES "^/usr/lib/" "^/System/"
    RESOLVED_DEPENDENCIES_VAR found
    UNRESOLVED_DEPENDENCIES_VAR missing)

if(missing)
    message(FATAL_ERROR "Libraries the app needs that were not found: ${missing}")
endif()

get_filename_component(contents "${TWOPSTART_EXE}" DIRECTORY)
get_filename_component(contents "${contents}" DIRECTORY)
set(frameworks "${contents}/Frameworks")
file(MAKE_DIRECTORY "${frameworks}")

set(stray "")
set(libraries "")
foreach(library IN LISTS found)
    string(FIND "${library}" "${TWOPSTART_GST_LIB}/" at)
    if(NOT at EQUAL 0)
        list(APPEND stray "${library}")
        continue()
    endif()
    # Under the name the others ask for, whatever it links to.
    get_filename_component(name "${library}" NAME)
    file(REAL_PATH "${library}" real)
    file(COPY_FILE "${real}" "${frameworks}/${name}")
    list(APPEND libraries "${frameworks}/${name}")
endforeach()
if(stray)
    message(FATAL_ERROR "Libraries found outside GStreamer's folder, which the app will not carry: ${stray}")
endif()
list(LENGTH libraries count)
message(STATUS "GStreamer's libraries in the app: ${count}")

# Each file: one kind of Mac, its own places to look and no others, and a signature for here, but the
# program, which is signed with the app.
function(settle file rpath)
    execute_process(COMMAND lipo -archs "${file}" OUTPUT_VARIABLE archs OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(archs MATCHES " ")
        execute_process(COMMAND lipo -thin arm64 "${file}" -output "${file}" COMMAND_ERROR_IS_FATAL ANY)
    endif()
    # A signature it came with (MoltenVK has one) would no longer fit; it is signed again below.
    execute_process(COMMAND codesign --remove-signature "${file}" OUTPUT_QUIET ERROR_QUIET)
    execute_process(COMMAND otool -l "${file}" OUTPUT_VARIABLE commands)
    string(REGEX MATCHALL "cmd LC_RPATH\n[^\n]*\n[ \t]*path [^\n]*" places "${commands}")
    foreach(place IN LISTS places)
        string(REGEX REPLACE ".*path ([^\n]*) \\(offset [0-9]+\\).*" "\\1" path "${place}")
        execute_process(COMMAND install_name_tool -delete_rpath "${path}" "${file}" COMMAND_ERROR_IS_FATAL ANY)
    endforeach()
    execute_process(COMMAND install_name_tool -add_rpath "${rpath}" "${file}" COMMAND_ERROR_IS_FATAL ANY)
    if(NOT file STREQUAL TWOPSTART_EXE)
        execute_process(COMMAND codesign --force --sign - "${file}" OUTPUT_QUIET ERROR_QUIET COMMAND_ERROR_IS_FATAL ANY)
    endif()
    # Nothing it names may be anywhere but the app or macOS itself.
    execute_process(COMMAND otool -L "${file}" OUTPUT_VARIABLE names)
    string(REGEX MATCHALL "\n[ \t]+/[^ \n]+" absolute "${names}")
    foreach(name IN LISTS absolute)
        string(STRIP "${name}" name)
        if(NOT name MATCHES "^/(usr/lib|System)/")
            message(FATAL_ERROR "${file} names ${name}, which is outside the app")
        endif()
    endforeach()
endfunction()

foreach(library IN LISTS libraries)
    settle("${library}" "@loader_path")
endforeach()
foreach(plugin IN LISTS TWOPSTART_GST_PLUGINS)
    get_filename_component(name "${plugin}" NAME)
    settle("${contents}/Library/gstreamer-1.0/${name}" "@loader_path/../../Frameworks")
endforeach()
settle("${TWOPSTART_EXE}" "@executable_path/../Frameworks")
get_filename_component(app "${contents}" DIRECTORY)
execute_process(COMMAND codesign --force --sign - "${app}" OUTPUT_QUIET ERROR_QUIET COMMAND_ERROR_IS_FATAL ANY)
