# CabinetOS's noise-test.cpp holds nine things to account. Eight are the protocol. The ninth is the
# friend's sound queue, which lives in a file written on PipeWire (friend_sound.cpp) and so builds
# only on the console. Until the sound is built here, this writes a copy of the test without that
# ninth part: the include, the test itself, and its one call.
#
# It cuts by the lines that begin and end the part, not by line number, so a change inside the sound
# test does not matter. If any of those lines has gone, the build stops: a moved pin never quietly
# tests less than this says.

function(noise_test_without_sound in out)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${in}")
    file(READ "${in}" text)

    set(include_line "#include \"friend_sound.h\"\n")
    set(first_line "// The friend's sound queue (friend_sound.h)")
    set(after_it "}  // namespace\n\nint main(")
    set(its_call "    soundKeepsPace();\n")
    foreach(piece include_line first_line after_it its_call)
        string(FIND "${text}" "${${piece}}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR
                "noise-test.cpp has changed shape: \"${${piece}}\" is gone. "
                "Read the new file and bring cmake/NoiseTestWithoutSound.cmake up to it.")
        endif()
    endforeach()

    string(REPLACE "${include_line}" "" text "${text}")
    string(REPLACE "${its_call}" "" text "${text}")
    string(FIND "${text}" "${first_line}" from)
    string(FIND "${text}" "${after_it}" to)
    if(NOT from LESS to)
        message(FATAL_ERROR "noise-test.cpp has changed shape: the sound test is no longer its last part.")
    endif()
    string(SUBSTRING "${text}" 0 ${from} head)
    string(SUBSTRING "${text}" ${to} -1 tail)
    set(text "${head}${tail}")

    # Written only when it differs, so an unchanged test is not built again.
    set(old "")
    if(EXISTS "${out}")
        file(READ "${out}" old)
    endif()
    if(NOT "${old}" STREQUAL "${text}")
        file(WRITE "${out}" "${text}")
    endif()
endfunction()
