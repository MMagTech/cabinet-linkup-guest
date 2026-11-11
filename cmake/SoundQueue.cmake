# The friend's sound queue, taken out of CabinetOS's friend_sound.h and friend_sound.cpp, where it sits
# beside the PipeWire stream that plays it on the console. The queue and its pace are what the console
# measured and tuned (#654), so this app plays the very same queue, through SDL instead of PipeWire.
#
# sound_queue(<friend_sound.h> <friend_sound.cpp> <folder>) writes sound_queue.h and sound_queue.cpp:
# the SoundQueue class, its constants and its functions, word for word. They need only PipeWire's
# small ring buffer, which src/pipewire/spa/utils/ringbuffer.h stands in for.
#
# noise_test_with_queue(<noise-test.cpp> <out>) writes CabinetOS's Noise test with its ninth part, the
# sound queue held to a host's clock, reading the queue from sound_queue.h. It is the whole test.
#
# Both cut by the lines that begin and end each part, not by line number, so a change inside a part
# does not matter. If any of those lines has gone, the build stops: a moved pin never quietly
# builds less than this says.

function(_cut text first after out_var what)
    string(FIND "${text}" "${first}" from)
    string(FIND "${text}" "${after}" to)
    if(from EQUAL -1 OR to EQUAL -1 OR NOT from LESS to)
        message(FATAL_ERROR
            "${what} has changed shape around \"${first}\". "
            "Read the new file and bring cmake/SoundQueue.cmake up to it.")
    endif()
    math(EXPR length "${to} - ${from}")
    string(SUBSTRING "${text}" ${from} ${length} piece)
    set(${out_var} "${piece}" PARENT_SCOPE)
endfunction()

# Written only when it differs, so an unchanged file is not built again.
function(_write_if_changed path text)
    set(old "")
    if(EXISTS "${path}")
        file(READ "${path}" old)
    endif()
    if(NOT "${old}" STREQUAL "${text}")
        file(WRITE "${path}" "${text}")
    endif()
endfunction()

function(sound_queue header source folder)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${header}" "${source}")
    file(READ "${header}" h)
    file(READ "${source}" cpp)

    _cut("${h}" "// The queue and its pace, with no PipeWire in it"
         "// The PipeWire playback stream that drains a SoundQueue" class "friend_sound.h")
    _cut("${cpp}" "// The pace, in ppm of the friend's clock"
         "const pw_stream_events* streamEvents(" constants "friend_sound.cpp")
    _cut("${cpp}" "SoundQueue::SoundQueue(uint32_t targetFrames)"
         "FriendSound::~FriendSound()" functions "friend_sound.cpp")

    set(made "// Made by cmake/SoundQueue.cmake from CabinetOS's linkup/")
    string(CONCAT header_text
        "${made}friend_sound.h. Never edited.\n\n#pragma once\n\n#include <spa/utils/ringbuffer.h>\n\n"
        "#include <atomic>\n#include <cstdint>\n#include <vector>\n\n${class}")
    string(CONCAT source_text
        "${made}friend_sound.cpp. Never edited.\n\n#include \"sound_queue.h\"\n\n"
        "#include <algorithm>\n#include <cstring>\n\nnamespace {\n\n${constants}}  // namespace\n\n${functions}")
    _write_if_changed("${folder}/sound_queue.h" "${header_text}")
    _write_if_changed("${folder}/sound_queue.cpp" "${source_text}")
endfunction()

function(noise_test_with_queue in out)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${in}")
    file(READ "${in}" text)
    set(include_line "#include \"friend_sound.h\"\n")
    string(FIND "${text}" "${include_line}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR
            "noise-test.cpp has changed shape: \"${include_line}\" is gone. "
            "Read the new file and bring cmake/SoundQueue.cmake up to it.")
    endif()
    string(REPLACE "${include_line}" "#include \"sound_queue.h\"\n" text "${text}")
    _write_if_changed("${out}" "${text}")
endfunction()
