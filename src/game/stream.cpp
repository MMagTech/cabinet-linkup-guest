// The friend's side of a game's stream (stream.h). Each part names what it follows in CabinetOS's
// stream_friend.cpp, so the two can be read side by side.

#include "game/stream.h"

#ifdef __APPLE__
#include <VideoToolbox/VideoToolbox.h>
#endif

#include <cstdio>
#include <cstring>

namespace game {

namespace {

constexpr int64_t kNoPictureMs = 150;
constexpr guint kSoundCushionMs = 60;   // stream_friend.h, "The sound is played as it arrives, into a queue"

bool hasElement(const char* name) {
    GstElementFactory* f = gst_element_factory_find(name);
    if (!f) return false;
    gst_object_unref(f);
    return true;
}

// The computer's own decoder, its graphics chip's: VideoToolbox on a Mac, Direct3D 11 on Windows,
// which nearly every Mac and PC has for H.264, and most from about 2016 on for H.265. There is no
// other: one in software would bring ffmpeg into the app.
//
// Another system names its own chip's decoder here. Until it does, GStreamer has no part of this
// name, so lacking() lists it and no game is offered, as none.cpp does.
#if defined(_WIN32)
constexpr const char* kDecoder = "d3d11h264dec";
constexpr const char* kDecoderH265 = "d3d11h265dec";   // there only when the chip decodes H.265
#elif defined(__APPLE__)
constexpr const char* kDecoder = "vtdec";
constexpr const char* kDecoderH265 = "vtdec";
#else
constexpr const char* kDecoder = "a decoder for this system, which stream.cpp does not name yet";
constexpr const char* kDecoderH265 = kDecoder;
#endif

std::string decoderFor(bool h265) {
    const char* name = h265 ? kDecoderH265 : kDecoder;
    return hasElement(name) ? name : "";
}

}  // namespace

// ---- Pictures ----------------------------------------------------------------

Pictures::~Pictures() { clear(); }

void Pictures::put(GstSample* sample) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (newest_) gst_sample_unref(newest_);
        newest_ = gst_sample_ref(sample);
    }
    if (wake_) wake_();
}

bool Pictures::take(const std::function<void(const Planes&)>& use) {
    GstSample* sample = nullptr;
    {
        std::lock_guard<std::mutex> lk(mu_);
        sample = newest_;
        newest_ = nullptr;
    }
    if (!sample) return false;
    GstVideoInfo info;
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    bool shown = false;
    if (buffer && caps && gst_video_info_from_caps(&info, caps) && GST_VIDEO_INFO_FORMAT(&info) == GST_VIDEO_FORMAT_NV12) {
        GstVideoFrame frame;
        if (gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ)) {
            Planes p;
            p.width = GST_VIDEO_FRAME_WIDTH(&frame);
            p.height = GST_VIDEO_FRAME_HEIGHT(&frame);
            p.y = static_cast<const uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));
            p.yPitch = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
            p.uv = static_cast<const uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 1));
            p.uvPitch = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 1);
            use(p);
            gst_video_frame_unmap(&frame);
            shown = true;
        }
    }
    gst_sample_unref(sample);
    return shown;
}

void Pictures::clear() {
    std::lock_guard<std::mutex> lk(mu_);
    if (newest_) gst_sample_unref(newest_);
    newest_ = nullptr;
}

// ---- The stream ----------------------------------------------------------------

// stream_friend.cpp's canDecodeH265.
bool Stream::canDecodeH265() {
    // For trying the two side by side: TWOPSTART_H264=1 says H.264 only.
    if (g_getenv("TWOPSTART_H264")) return false;
#ifdef __APPLE__
    // VideoToolbox takes H.265 on every Mac, but only the chip's own is fast enough.
    if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC)) return false;
#endif
    return !decoderFor(true).empty() && hasElement("rtph265depay") && hasElement("h265parse");
}

std::vector<std::string> Stream::lacking() {
    // webrtcbin and what it makes inside it: the connection (nice), what locks it (dtls, srtp), the
    // data channels (sctp), and the packets' order, resends and repairs (rtpmanager, rtp). Then the
    // picture and the sound, as onPadAdded builds them.
    static const char* const kParts[] = {
        "webrtcbin", "nicesrc", "nicesink", "dtlssrtpenc", "dtlssrtpdec", "srtpenc", "srtpdec", "sctpenc",
        "sctpdec", "rtpbin", "rtpfunnel", "rtprtxreceive", "rtpulpfecdec", "rtpreddec", "rtph264depay",
        "h264parse", kDecoder, "rtph265depay", "h265parse", "queue", "appsink", "rtpopusdepay", "opusdec",
        "capsfilter",
    };
    std::vector<std::string> out;
    for (const char* part : kParts)
        if (!hasElement(part)) out.emplace_back(part);
    return out;
}

std::vector<std::string> Stream::unloadable() {
    std::vector<std::string> out;
    GList* plugins = gst_registry_get_plugin_list(gst_registry_get());
    for (GList* p = plugins; p; p = p->next) {
        GstPlugin* plugin = GST_PLUGIN(p->data);
        if (GST_OBJECT_FLAG_IS_SET(plugin, GST_PLUGIN_FLAG_BLACKLISTED)) out.emplace_back(gst_plugin_get_name(plugin));
    }
    gst_plugin_list_free(plugins);
    return out;
}

Stream::Stream(Rtc::Hooks hooks, std::function<void()> askFullPicture, Pictures& pictures)
    : hooks_(std::move(hooks)), askFull_(std::move(askFullPicture)), pictures_(pictures) {}

Stream::~Stream() { stop(); }

gboolean Stream::onBus(GstBus*, GstMessage* m, gpointer) {
    if (GST_MESSAGE_TYPE(m) == GST_MESSAGE_WARNING || GST_MESSAGE_TYPE(m) == GST_MESSAGE_ERROR) {
        GError* e = nullptr;
        if (GST_MESSAGE_TYPE(m) == GST_MESSAGE_WARNING) gst_message_parse_warning(m, &e, nullptr);
        else gst_message_parse_error(m, &e, nullptr);
        std::fprintf(stderr, "[game] %s: %s\n", GST_OBJECT_NAME(m->src), e ? e->message : "");
        g_clear_error(&e);
    }
    return TRUE;
}

GstPadProbeReturn Stream::onPacket(GstPad*, GstPadProbeInfo*, gpointer self) {
    static_cast<Stream*>(self)->lastPacketMs_ = Rtc::nowMs();
    return GST_PAD_PROBE_OK;
}

// onSound: the decoder's sound into the queue, on GStreamer's thread.
GstFlowReturn Stream::onSound(GstAppSink* sink, gpointer self) {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (buffer && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        static_cast<Stream*>(self)->sound_.push(reinterpret_cast<const int16_t*>(map.data),
                                                static_cast<uint32_t>(map.size / (SoundQueue::kChannels * sizeof(int16_t))));
        gst_buffer_unmap(buffer, &map);
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// In place of glimagesink: the newest picture, for the window.
GstFlowReturn Stream::onShown(GstAppSink* sink, gpointer self) {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;
    static_cast<Stream*>(self)->pictures_.put(sample);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

GstPadProbeReturn Stream::onPicture(GstPad*, GstPadProbeInfo*, gpointer self) {
    auto* me = static_cast<Stream*>(self);
    me->lastPictureMs_ = Rtc::nowMs();
    ++me->decoded_;
    return GST_PAD_PROBE_OK;
}

void Stream::onPadAdded(GstElement*, GstPad* pad, gpointer self) {
    auto* me = static_cast<Stream*>(self);
    if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC) return;
    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) caps = gst_pad_query_caps(pad, nullptr);
    const GstStructure* s = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(s, "media");
    const gchar* encoding = gst_structure_get_string(s, "encoding-name");
    const bool video = media && std::strcmp(media, "video") == 0;
    const bool h265 = encoding && std::strcmp(encoding, "H265") == 0;
    std::string description;
    if (video) {
        guint ssrc = 0;
        if (gst_structure_get_uint(s, "ssrc", &ssrc)) me->pictureSsrc_ = ssrc;
        const std::string dec = decoderFor(h265);
        if (dec.empty() || !encoding || (!h265 && std::strcmp(encoding, "H264") != 0)) {
            std::fprintf(stderr, "[game] no decoder for that picture\n");
            gst_caps_unref(caps);
            return;
        }
        std::fprintf(stderr, "[game] the picture is %s, through %s\n", h265 ? "H.265" : "H.264", dec.c_str());
        description = std::string(h265 ? "rtph265depay name=depay request-keyframe=true wait-for-keyframe=true ! h265parse ! "
                                       : "rtph264depay name=depay request-keyframe=true wait-for-keyframe=true ! h264parse ! ") +
                      dec + " name=dec ! queue max-size-buffers=1 max-size-bytes=0 max-size-time=0 leaky=downstream ! "
                            "appsink name=picture caps=video/x-raw,format=NV12 sync=false max-buffers=1 drop=true";
    } else {
        guint ssrc = 0;
        if (gst_structure_get_uint(s, "ssrc", &ssrc)) me->cushionSound(ssrc);
        else std::fprintf(stderr, "[game] the sound came with no SSRC; its cushion stays at the picture's\n");
        // Unplayed, it still has to be taken: a queue nobody empties drops what does not fit.
        if (!me->sound_.start()) std::fprintf(stderr, "[game] the sound will not be heard\n");
        description = "rtpopusdepay ! opusdec plc=true ! audio/x-raw,format=S16LE,rate=48000,channels=2 ! "
                      "appsink name=sound sync=false async=false";
    }
    gst_caps_unref(caps);
    GError* error = nullptr;
    GstElement* bin = gst_parse_bin_from_description(description.c_str(), TRUE, &error);
    if (!bin) {
        std::fprintf(stderr, "[game] %s\n", error ? error->message : "");
        g_clear_error(&error);
        return;
    }
    // Before the first piece can arrive, or appsink keeps what came first as delay for good.
    if (GstElement* sink = gst_bin_get_by_name(GST_BIN(bin), video ? "picture" : "sound")) {
        GstAppSinkCallbacks callbacks{};
        callbacks.new_sample = video ? onShown : onSound;
        gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, me, nullptr);
        gst_object_unref(sink);
    }
    gst_bin_add(GST_BIN(me->pipeline_), bin);
    gst_element_sync_state_with_parent(bin);
    GstPad* sink = gst_element_get_static_pad(bin, "sink");
    if (gst_pad_link(pad, sink) != GST_PAD_LINK_OK) std::fprintf(stderr, "[game] could not link the %s\n", video ? "picture" : "sound");
    gst_object_unref(sink);
    if (video) {
        if (GstElement* depay = gst_bin_get_by_name(GST_BIN(bin), "depay")) {
            GstPad* in = gst_element_get_static_pad(depay, "sink");
            gst_pad_add_probe(in, GST_PAD_PROBE_TYPE_BUFFER, onPacket, me, nullptr);
            gst_object_unref(in);
            gst_object_unref(depay);
        }
        if (GstElement* dec = gst_bin_get_by_name(GST_BIN(bin), "dec")) {
            GstPad* out = gst_element_get_static_pad(dec, "src");
            gst_pad_add_probe(out, GST_PAD_PROBE_TYPE_BUFFER, onPicture, me, nullptr);
            gst_object_unref(out);
            gst_object_unref(dec);
        }
    }
}

void Stream::onNewTransceiver(GstElement*, GstWebRTCRTPTransceiver* t, gpointer) {
    g_object_set(t, "do-nack", TRUE, nullptr);
}

// Every stream gets a jitter buffer as its first packet arrives, before its pad: kept by SSRC until the
// pad says which one is the sound.
void Stream::onNewJitterbuffer(GstElement*, GstElement* jitterbuffer, guint, guint ssrc, gpointer self) {
    auto* me = static_cast<Stream*>(self);
    std::lock_guard<std::mutex> lk(me->jitterMu_);
    auto it = me->jitterbuffers_.find(ssrc);
    if (it != me->jitterbuffers_.end()) gst_object_unref(it->second);
    me->jitterbuffers_[ssrc] = GST_ELEMENT(gst_object_ref(jitterbuffer));
}

void Stream::cushionSound(guint ssrc) {
    std::lock_guard<std::mutex> lk(jitterMu_);
    auto it = jitterbuffers_.find(ssrc);
    if (it == jitterbuffers_.end()) {
        std::fprintf(stderr, "[game] no jitter buffer for the sound; its cushion stays at the picture's\n");
        return;
    }
    // Lost is said downstream, so the decoder fills the hole in.
    g_object_set(it->second, "latency", kSoundCushionMs, "do-lost", TRUE, nullptr);
}

Stream::Losses Stream::pictureLosses() {
    Losses out;
    std::lock_guard<std::mutex> lk(jitterMu_);
    auto it = jitterbuffers_.find(pictureSsrc_.load());
    if (it == jitterbuffers_.end()) return out;
    GstStructure* stats = nullptr;
    g_object_get(it->second, "stats", &stats, nullptr);
    if (!stats) return out;
    gst_structure_get_uint64(stats, "num-pushed", &out.in);
    gst_structure_get_uint64(stats, "num-lost", &out.lost);
    gst_structure_get_uint64(stats, "num-late", &out.late);
    gst_structure_get_uint64(stats, "num-duplicates", &out.duplicates);
    gst_structure_get_uint64(stats, "rtx-count", &out.resent);
    gst_structure_get_uint64(stats, "rtx-success-count", &out.resentCame);
    gst_structure_free(stats);
    return out;
}

namespace {

// Whether a description names a resend stream (rtx) for the picture.
bool namesResends(const GstWebRTCSessionDescription* d) {
    if (!d || !d->sdp) return false;
    for (guint i = 0; i < gst_sdp_message_medias_len(d->sdp); ++i) {
        const GstSDPMedia* m = gst_sdp_message_get_media(d->sdp, i);
        if (g_strcmp0(gst_sdp_media_get_media(m), "video") != 0) continue;
        for (guint a = 0;; ++a) {
            const gchar* v = gst_sdp_media_get_attribute_val_n(m, "rtpmap", a);
            if (!v) break;
            if (g_strrstr(v, "rtx/")) return true;
        }
    }
    return false;
}

}  // namespace

void Stream::sayResends() {
    if (!webrtc_) return;
    GstWebRTCSessionDescription *offer = nullptr, *answer = nullptr;
    g_object_get(webrtc_, "remote-description", &offer, "local-description", &answer, nullptr);
    std::fprintf(stderr, "[game] resends for the picture: the host %s them, this app %s\n",
                 namesResends(offer) ? "offers" : "does not offer", namesResends(answer) ? "takes them" : "does not");
    if (offer) gst_webrtc_session_description_free(offer);
    if (answer) gst_webrtc_session_description_free(answer);
}

Stream::Resends Stream::resends() {
    Resends out;
    if (!pipeline_) return out;
    GstIterator* it = gst_bin_iterate_recurse(GST_BIN(pipeline_));
    GValue item = G_VALUE_INIT;
    while (gst_iterator_next(it, &item) == GST_ITERATOR_OK) {
        GstElement* e = GST_ELEMENT(g_value_get_object(&item));
        GstElementFactory* f = gst_element_get_factory(e);
        if (f && g_strcmp0(GST_OBJECT_NAME(f), "rtprtxreceive") == 0) {
            guint asked = 0, came = 0, matched = 0;
            g_object_get(e, "num-rtx-requests", &asked, "num-rtx-packets", &came, "num-rtx-assoc-packets", &matched, nullptr);
            out.receiver = true;
            out.asked += asked;
            out.came += came;
            out.matched += matched;
        }
        g_value_reset(&item);
    }
    g_value_unset(&item);
    gst_iterator_free(it);
    return out;
}

gboolean Stream::onWatchdog(gpointer self) {
    auto* me = static_cast<Stream*>(self);
    const int64_t now = Rtc::nowMs();
    const int64_t packet = me->lastPacketMs_.load();
    const int64_t picture = me->lastPictureMs_.load();
    // Packets are arriving and nothing is coming out. Quiet from the host is not this: it may have
    // stopped sending because something else is on its screen.
    if (packet != 0 && now - packet < kNoPictureMs && now - picture > kNoPictureMs && now - me->lastAskMs_ > kNoPictureMs) {
        me->lastAskMs_ = now;
        ++me->asks_;
        if (me->askFull_) me->askFull_();
        // What came before it, to tell a loss on the way from the decoder.
        const Losses l = me->pictureLosses();
        std::fprintf(stderr, "[game] at %.1f s, no picture for %lld ms: asked for a full one (%llu picture packets lost so far, "
                             "%llu late)\n",
                     static_cast<double>(now - me->startedMs_) / 1000.0, static_cast<long long>(now - picture),
                     static_cast<unsigned long long>(l.lost), static_cast<unsigned long long>(l.late));
    }
    return G_SOURCE_CONTINUE;
}

bool Stream::start(const Rtc::Paths& paths) {
    pipeline_ = gst_pipeline_new("friend");
    webrtc_ = gst_element_factory_make("webrtcbin", "w");
    if (!webrtc_) {
        std::fprintf(stderr, "[game] no webrtcbin\n");
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        return false;
    }
    g_object_set(webrtc_, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, "latency", 100u, nullptr);
    gst_bin_add(GST_BIN(pipeline_), webrtc_);
    rtpbin_ = gst_bin_get_by_name(GST_BIN(webrtc_), "rtpbin");
    if (rtpbin_) g_signal_connect(rtpbin_, "new-jitterbuffer", G_CALLBACK(onNewJitterbuffer), this);
    gst_bus_add_watch(GST_ELEMENT_BUS(pipeline_), onBus, this);
    rtc_ = std::make_unique<Rtc>(webrtc_, /*offerer=*/false, hooks_);
    rtc_->configure(paths);
    g_signal_connect(webrtc_, "pad-added", G_CALLBACK(onPadAdded), this);
    g_signal_connect(webrtc_, "on-new-transceiver", G_CALLBACK(onNewTransceiver), this);
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    lastPacketMs_ = 0;
    lastPictureMs_ = Rtc::nowMs();
    startedMs_ = lastPictureMs_.load();
    watchdog_ = g_timeout_add(20, onWatchdog, this);
    return true;
}

void Stream::stop() {
    if (watchdog_) g_source_remove(watchdog_);
    watchdog_ = 0;
    if (!pipeline_) return;
    std::fprintf(stderr, "[game] %u pictures decoded, asked for a full one %u times\n", decoded_.load(), asks_);
    const Losses l = pictureLosses();
    std::fprintf(stderr, "[game] picture packets: %llu in, %llu lost, %llu late, %llu twice, %llu resends asked, %llu came in time\n",
                 static_cast<unsigned long long>(l.in), static_cast<unsigned long long>(l.lost),
                 static_cast<unsigned long long>(l.late), static_cast<unsigned long long>(l.duplicates),
                 static_cast<unsigned long long>(l.resent), static_cast<unsigned long long>(l.resentCame));
    const Resends r = resends();
    if (r.receiver)
        std::fprintf(stderr, "[game] resends: %u asked of the host, %u came at all, %u matched to an ask\n", r.asked, r.came, r.matched);
    else
        std::fprintf(stderr, "[game] resends: none can come, there is no resend receiver\n");
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    if (sound_.running()) {
        const SoundQueue::Stats s = sound_.stats();
        std::fprintf(stderr, "[game] sound ran dry %u times, held at %+.0f ppm, least in hand %.0f ms, "
                             "dropped %u times over and %u for no room\n",
                     s.underruns, s.trimPpm, s.lowMs, s.tooFull, s.noRoom);
    }
    sound_.stop();
    g_signal_handlers_disconnect_by_data(webrtc_, this);
    if (rtpbin_) {
        g_signal_handlers_disconnect_by_data(rtpbin_, this);
        gst_object_unref(rtpbin_);
        rtpbin_ = nullptr;
    }
    {
        std::lock_guard<std::mutex> lk(jitterMu_);
        for (auto& [ssrc, jb] : jitterbuffers_) gst_object_unref(jb);
        jitterbuffers_.clear();
    }
    rtc_.reset();
    gst_bus_remove_watch(GST_ELEMENT_BUS(pipeline_));
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
    webrtc_ = nullptr;
    pictures_.clear();
}

}  // namespace game
