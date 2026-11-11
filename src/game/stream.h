// The friend's side of a game's stream: receive, decode, and hand the picture to the app's window
// and the sound to the app's player. It is CabinetOS's stream_friend.cpp, line for line where it can
// be, built for a computer: that file shows the picture in a GStreamer window of its own
// (glimagesink) and plays the sound through PipeWire, and here both belong to the app, so panels can
// be drawn over the game.
//
// WHAT IS THE CONSOLE'S, AS IT IS (stream_friend.h says why for each, all measured, #654):
// webrtcbin's latency at 100 ms, resends asked for, the sound's jitter buffer waiting up to 60 ms for
// a missing piece before Opus fills it in, the one-picture queue before the picture leaves, and the
// watchdog asking the host for a full picture while packets arrive and no picture comes out.
//
// WHAT IS THIS APP'S: the decoder is the computer's own, VideoToolbox on a Mac, and the picture is
// kept in memory, the newest only, for the window to draw (Pictures). The colours are BT.601 limited
// range, as the console states after its decoder; the window's texture says so (app.cpp).

#pragma once

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/webrtc/webrtc.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "game/game.h"
#include "game/sound.h"
#include "rtc.h"

namespace game {

// The newest decoded picture. The decoder's thread puts; the window's thread takes the newest it has
// not drawn. An older one waiting is dropped, as the console's one-frame queue drops it.
class Pictures {
public:
    ~Pictures();
    // The decoder's thread. Takes its own reference. `wake` is called after, to stop the window sleeping.
    void put(GstSample* sample);
    // The window's thread: true, with `use` called on the picture while it is held, when there is one
    // it has not had.
    bool take(const std::function<void(const Planes&)>& use);
    void clear();
    void setWake(std::function<void()> wake) { wake_ = std::move(wake); }

private:
    std::mutex mu_;
    GstSample* newest_ = nullptr;
    std::function<void()> wake_;
};

class Stream {
public:
    Stream(Rtc::Hooks hooks, std::function<void()> askFullPicture, Pictures& pictures);
    ~Stream();

    bool start(const Rtc::Paths& paths);
    void stop();
    Rtc* rtc() { return rtc_.get(); }

    // Whether this computer can decode the host's picture: H.264, with a decoder GStreamer has.
    static bool canDecodeH264();

private:
    static void onPadAdded(GstElement*, GstPad* pad, gpointer self);
    static void onNewTransceiver(GstElement*, GstWebRTCRTPTransceiver* t, gpointer self);
    static void onNewJitterbuffer(GstElement*, GstElement* jitterbuffer, guint session, guint ssrc, gpointer self);
    void cushionSound(guint ssrc);
    static gboolean onBus(GstBus*, GstMessage* m, gpointer self);
    static GstPadProbeReturn onPacket(GstPad*, GstPadProbeInfo*, gpointer self);
    static GstPadProbeReturn onPicture(GstPad*, GstPadProbeInfo*, gpointer self);
    static GstFlowReturn onSound(GstAppSink* sink, gpointer self);
    static GstFlowReturn onShown(GstAppSink* sink, gpointer self);
    static gboolean onWatchdog(gpointer self);

    Rtc::Hooks hooks_;
    std::function<void()> askFull_;
    Pictures& pictures_;
    GstElement* pipeline_ = nullptr;
    GstElement* webrtc_ = nullptr;
    std::unique_ptr<Rtc> rtc_;
    GstElement* rtpbin_ = nullptr;
    std::mutex jitterMu_;
    std::map<guint, GstElement*> jitterbuffers_;   // by SSRC, each held by a reference
    guint watchdog_ = 0;
    std::atomic<int64_t> lastPacketMs_{0};
    std::atomic<int64_t> lastPictureMs_{0};
    int64_t lastAskMs_ = 0;
    std::atomic<uint32_t> decoded_{0};
    uint32_t asks_ = 0;
    Sound sound_;
};

}  // namespace game
