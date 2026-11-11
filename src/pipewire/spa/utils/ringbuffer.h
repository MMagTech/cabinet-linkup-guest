// What the console's sound queue (sound_queue.h, cut from CabinetOS's friend_sound.h) takes from
// PipeWire: its small ring buffer, here on a computer with no PipeWire, as src/win/ stands in for
// what Windows lacks. The same names and the same rules as PipeWire's own spa/utils/ringbuffer.h.
//
// ONE WRITER AND ONE READER, each on a thread of its own, and no lock: each side moves only its own
// index, and reads the other's. An index counts frames forever and wraps at 2^32; the space between
// the two is what is in the buffer. A side's data is copied before it moves its index, which
// publishes it (release), and the other side reads that index before it reads the data (acquire).

#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

struct spa_ringbuffer {
    uint32_t readindex;
    uint32_t writeindex;
};

inline void spa_ringbuffer_init(spa_ringbuffer* rbuf) {
    std::atomic_ref<uint32_t>(rbuf->readindex).store(0, std::memory_order_relaxed);
    std::atomic_ref<uint32_t>(rbuf->writeindex).store(0, std::memory_order_relaxed);
}

// The reader's index, and how much is there to read.
inline int32_t spa_ringbuffer_get_read_index(spa_ringbuffer* rbuf, uint32_t* index) {
    *index = std::atomic_ref<uint32_t>(rbuf->readindex).load(std::memory_order_relaxed);
    return static_cast<int32_t>(std::atomic_ref<uint32_t>(rbuf->writeindex).load(std::memory_order_acquire) - *index);
}

// The writer's index, and how much is there already.
inline int32_t spa_ringbuffer_get_write_index(spa_ringbuffer* rbuf, uint32_t* index) {
    *index = std::atomic_ref<uint32_t>(rbuf->writeindex).load(std::memory_order_relaxed);
    return static_cast<int32_t>(*index - std::atomic_ref<uint32_t>(rbuf->readindex).load(std::memory_order_acquire));
}

// `len` bytes out of a buffer of `size` bytes from `offset`, carrying on at its start.
inline void spa_ringbuffer_read_data(spa_ringbuffer*, const void* buffer, uint32_t size, uint32_t offset, void* data,
                                     uint32_t len) {
    const uint32_t first = std::min(len, size - offset);
    std::memcpy(data, static_cast<const uint8_t*>(buffer) + offset, first);
    if (len > first) std::memcpy(static_cast<uint8_t*>(data) + first, buffer, len - first);
}

inline void spa_ringbuffer_write_data(spa_ringbuffer*, void* buffer, uint32_t size, uint32_t offset, const void* data,
                                      uint32_t len) {
    const uint32_t first = std::min(len, size - offset);
    std::memcpy(static_cast<uint8_t*>(buffer) + offset, data, first);
    if (len > first) std::memcpy(buffer, static_cast<const uint8_t*>(data) + first, len - first);
}

inline void spa_ringbuffer_read_update(spa_ringbuffer* rbuf, int32_t index) {
    std::atomic_ref<uint32_t>(rbuf->readindex).store(static_cast<uint32_t>(index), std::memory_order_release);
}

inline void spa_ringbuffer_write_update(spa_ringbuffer* rbuf, int32_t index) {
    std::atomic_ref<uint32_t>(rbuf->writeindex).store(static_cast<uint32_t>(index), std::memory_order_release);
}
