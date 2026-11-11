// The game's sound (sound.h).

#include "game/sound.h"

#include <cstdio>

namespace game {

namespace {

constexpr int kFrameBytes = SoundQueue::kChannels * static_cast<int>(sizeof(int16_t));

}  // namespace

Sound::~Sound() { stop(); }

bool Sound::start() {
    if (stream_) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "[game] no sound: %s\n", SDL_GetError());
        return false;
    }
    // 10 ms a cycle, one Opus piece, as the console asks of PipeWire (PW_KEY_NODE_LATENCY 480/48000).
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "480");
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = SoundQueue::kChannels;
    spec.freq = SoundQueue::kRate;
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, onWant, this);
    if (!stream_) {
        std::fprintf(stderr, "[game] the sound would not open: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

// SDL's audio thread: exactly what it asks for, out of the queue, and the queue's pace given to its
// resampler. No locks of this program's own, and no waiting.
void SDLCALL Sound::onWant(void* self, SDL_AudioStream* stream, int additional, int) {
    auto* me = static_cast<Sound*>(self);
    const uint32_t frames = static_cast<uint32_t>(additional > 0 ? additional / kFrameBytes : 0);
    if (frames == 0) return;
    me->pulled_.resize(static_cast<size_t>(frames) * SoundQueue::kChannels);
    me->queue_.pull(me->pulled_.data(), frames);
    SDL_PutAudioStreamData(stream, me->pulled_.data(), static_cast<int>(frames) * kFrameBytes);
    const double rate = me->queue_.rate();
    if (rate != me->rateGiven_ && SDL_SetAudioStreamFrequencyRatio(stream, static_cast<float>(1.0 / rate)))
        me->rateGiven_ = rate;
}

void Sound::stop() {
    if (!stream_) return;
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

}  // namespace game
