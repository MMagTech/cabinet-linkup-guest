// The game's sound, played through SDL at the host's pace: the console's own sound queue
// (SoundQueue, cut from CabinetOS's friend_sound.h by cmake/SoundQueue.cmake), drained by SDL's
// audio thread where the console's is drained by PipeWire's.
//
// WHY THE CONSOLE'S QUEUE: the host's sound clock is not this computer's, and a queue played at this
// computer's pace runs dry or overflows within a minute (friend_sound.h, measured on the console).
// The queue keeps 40 ms in hand, more after a stall, and says each second how much faster or slower
// to play to stay there. The console gives that to PipeWire's resampler (pw_stream_set_rate); here it
// is SDL's (SDL_SetAudioStreamFrequencyRatio), which is the same thing the other way up: a ratio
// above 1 plays faster and takes more of the queue.

#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <vector>

#include "sound_queue.h"

namespace game {

class Sound {
public:
    ~Sound();

    // Plays silence until the queue holds its target. Any thread; a second call does nothing.
    bool start();
    // GStreamer's thread. Never waits.
    void push(const int16_t* frames, uint32_t count) { queue_.push(frames, count); }
    // After the pipeline that pushes has stopped.
    void stop();
    bool running() const { return stream_ != nullptr; }
    SoundQueue::Stats stats() const { return queue_.stats(); }

private:
    static void SDLCALL onWant(void* self, SDL_AudioStream* stream, int additional, int total);

    SoundQueue queue_;
    SDL_AudioStream* stream_ = nullptr;
    std::vector<int16_t> pulled_;   // SDL's audio thread only
    double rateGiven_ = 1.0;        // SDL's audio thread only
};

}  // namespace game
