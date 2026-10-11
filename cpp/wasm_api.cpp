// C ABI over Session for the browser (WASM) host. Also compiled natively by
// cpp/test_wasm_api.cpp so the shim is tested without a browser.
//
// Contract: the host fills ms_rooms() (count x 18 floats: distance seconds, speed,
// damping, swell flag, lowpass Hz, highpass Hz, spread, diffusion, predelay seconds, reflection pattern, modulation seconds/Hz, shimmer, hold, drive, ducking, pan, bloom seconds) and ms_probabilities() (count x
// count row-major), then calls ms_prepare. Audio is produced by ms_process into
// ms_left()/ms_right(). Telemetry is a flat float snapshot (see MS_T_* below).
// All buffers are preallocated inside the module; no allocation during ms_process.
#include "session.hpp"
#include <cstring>
#include <memory>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define MS_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define MS_EXPORT
#endif

namespace {
using namespace manyspace;
constexpr std::size_t kMaxFrames = 4096;
constexpr std::size_t kRoomFloats = 18;
enum : int { MS_T_CURSOR, MS_T_ROOM, MS_T_ENVELOPE, MS_T_THRESHOLD, MS_T_PEAKL, MS_T_PEAKR, MS_T_GRAIN,
             MS_T_PLAYING, MS_T_INSTANCES, MS_T_FRAMES, MS_T_RATE, MS_T_LOOPFRAMES, MS_T_PASSES, MS_T_VOICES };
constexpr int kVoiceFloats = 7; // instance, room, age, gain, level, active, pending
constexpr int kTelemetryFloats = MS_T_VOICES + 8*kVoiceFloats;

struct State {
    std::unique_ptr<Session> session;
    float rate = 48000;
    bool prepared = false;
    std::array<float, maxRooms*kRoomFloats> rooms{};
    std::array<float, maxRooms*maxRooms> probabilities{};
    std::array<float, kMaxFrames> left{}, right{};
    std::array<float, kTelemetryFloats> telemetry{};
    std::array<char, 256> error{};
    std::size_t frames = 0, loopFrames = 0;
    std::array<float,128*3> events{};int eventCount=0;
    int roomCount = 0;
    std::vector<float> source; // interleaved stereo, allocated only off the callback
    std::size_t sourceFrames=0;
};
State& state() { static State s; return s; }
void setError(const char* message) { std::strncpy(state().error.data(), message, state().error.size()-1); }
}

extern "C" {
MS_EXPORT int ms_create(float sampleRate) {
    auto& s = state();
    if (!(sampleRate >= 8000 && sampleRate <= 96000)) { setError("Unsupported sample rate"); return 0; }
    s.session = std::make_unique<Session>(); s.rate = sampleRate; s.prepared = false; s.error[0] = 0;
    return 1;
}
MS_EXPORT float* ms_rooms() { return state().rooms.data(); }
MS_EXPORT float* ms_probabilities() { return state().probabilities.data(); }
MS_EXPORT float* ms_left() { return state().left.data(); }
MS_EXPORT float* ms_right() { return state().right.data(); }
MS_EXPORT float* ms_telemetry() { return state().telemetry.data(); }
MS_EXPORT const char* ms_error() { return state().error.data(); }
MS_EXPORT int ms_max_frames() { return static_cast<int>(kMaxFrames); }
MS_EXPORT int ms_telemetry_floats() { return kTelemetryFloats; }
MS_EXPORT int ms_max_rooms() { return static_cast<int>(maxRooms); }
MS_EXPORT int ms_source_max_frames() { return 1000000; } // 8 MB staging + 8 MB Clip in the fixed heap
MS_EXPORT float* ms_source_ptr(int frames) {
    auto& s=state();
    if (frames<1 || frames>ms_source_max_frames()) { setError("Audio exceeds the custom clip limit"); return nullptr; }
    try { s.source.resize(static_cast<std::size_t>(frames)*2); s.sourceFrames=frames; }
    catch (...) { setError("Not enough memory for custom audio"); return nullptr; }
    return s.source.data();
}
MS_EXPORT float* ms_events() { return state().events.data(); }
MS_EXPORT int ms_event_count() { return state().eventCount; }
MS_EXPORT int ms_dropped_events() { return state().session ? state().session->coreForOfflineTests().droppedChanges():0; }
MS_EXPORT int ms_room_floats() { return static_cast<int>(kRoomFloats); }

// example: 0 plucks, 1 drums, 2 chords. treatment: 0 galaxy, 1 normal, 2 reverse, 3 half.
// traversal: 0 next, 1 leap. Returns 1 on success; ms_error() explains failure.
MS_EXPORT int ms_prepare(int count, float mix, float decay, float sensitivity, int automatic, int traversal,
                         int budget, unsigned seed, int example, int treatment, int phraseNext, int loop, int room) {
    auto& s = state();
    s.prepared = false;
    if (!s.session) { setError("Call ms_create first"); return 0; }
    if (count != 16 && count != 64 && count != 256) { setError("Use 16, 64 or 256 rooms"); return 0; }
    if (automatic<0 || automatic>2) { setError("Invalid trigger mode"); return 0; }
    if (budget != 4 && budget != 8) { setError("Use four or eight engines"); return 0; }
    if (!(mix >= 0 && mix <= 1) || !(decay >= .3f && decay <= 8) || !(sensitivity >= 0 && sensitivity <= 1)) { setError("Control out of range"); return 0; }
    if (example < 0 || example > 3 || treatment < 0 || treatment > 3 || room < 0 || room >= count) { setError("Invalid scene selection"); return 0; }
    Galaxy g; g.count = static_cast<std::size_t>(count);
    for (int i = 0; i < count; ++i) {
        const float* r = s.rooms.data() + i*kRoomFloats;
        Room room_{r[0], r[1], r[2], r[3] != 0, r[4], r[5], r[6],r[7],r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15],r[16],r[17]};
        if (!(r[3]==0 || r[3]==1) || !(room_.distance >= .015f && room_.distance <= .095f) || !(room_.speed >= .5f && room_.speed <= 1.5f)
            || !(room_.damping >= .2f && room_.damping <= .85f) || !(room_.lowpassHz >= 800 && room_.lowpassHz <= 16000)
            || !(room_.highpassHz >= 10 && room_.highpassHz <= 1000) || room_.highpassHz >= room_.lowpassHz
            || !(room_.spread >= 0 && room_.spread <= 1) || !room_.validCharacter()) {
            setError("Room property out of range"); return 0;
        }
        g.rooms[i] = room_;
        double sum = 0;
        for (int j = 0; j < count; ++j) {
            float p = s.probabilities[i*count + j];
            if (!(p >= 0 && p <= 1)) { setError("Probability out of range"); return 0; }
            g.probabilities[i][j] = p; sum += p;
        }
        if (std::abs(sum-1) > 1e-6) { setError("Incomplete probability row"); return 0; }
    }
    Scenario scenario = scenarios()[2];
    scenario.example = static_cast<Example>(example); scenario.treatment = static_cast<Treatment>(treatment);
    scenario.mix = mix; scenario.decay = decay; scenario.sensitivity = sensitivity;
    scenario.automatic = automatic == 1; scenario.trigger=static_cast<Trigger>(automatic); scenario.traversal = traversal == 0 ? Traversal::Next : Traversal::Leap;
    scenario.phraseNext = phraseNext != 0;
    try {
        Clip clip;
        if (example==3) {
            if (!s.sourceFrames) throw std::invalid_argument("Load custom audio first");
            clip.sampleRate=s.rate; clip.bpm=0; clip.loopFrames=s.sourceFrames; clip.musicSeconds=s.sourceFrames/s.rate;
            clip.left.resize(s.sourceFrames);clip.right.resize(s.sourceFrames);
            for(std::size_t i=0;i<s.sourceFrames;++i) { clip.left[i]=s.source[2*i];clip.right[i]=s.source[2*i+1]; }
        } else clip = generateClip(scenario.example, s.rate);
        s.frames = clip.left.size(); s.loopFrames = clip.loopFrames;
        s.session->prepare(g, std::move(clip), scenario, static_cast<std::size_t>(budget), seed);
    } catch (const std::exception& e) { setError(e.what()); return 0; }
    if (room) s.session->queue.push({CommandKind::Room, static_cast<float>(room)});
    s.session->queue.push({CommandKind::Loop, loop ? 1.f : 0.f});
    s.prepared = true; s.roomCount=count; s.error[0] = 0;
    return 1;
}

// kind: 0 knobs(a=mix,b=decay,c=sensitivity) 1 next 2 leap 3 room(a) 4 automatic(a)
// 5 traversal(a: 0 next,1 leap) 6 play(a) 7 loop(a) 8 bus(a: 0 dry,1 effect,2 wet,3 immersion).
MS_EXPORT int ms_command(int kind, float a, float b, float c) {
    auto& s = state();
    if (!s.prepared) { setError("Audio is not ready"); return 0; }
    if (kind < 0 || kind > 8) { setError("Unknown audio command"); return 0; }
    if (kind == 0 && (!(a >= 0 && a <= 1) || !(b >= .3f && b <= 8) || !(c >= 0 && c <= 1))) { setError("Control out of range"); return 0; }
    if (kind==4 && (!(a>=0 && a<=2) || a!=std::floor(a))) { setError("Invalid trigger mode"); return 0; }
    if (kind==8 && (!(a>=0 && a<=3) || a!=std::floor(a))) { setError("Invalid audition bus"); return 0; }
    if (kind == 3 && (!(a >= 0 && a < s.roomCount) || a!=std::floor(a))) { setError("Room out of range"); return 0; }
    if (!s.session->queue.push({static_cast<CommandKind>(kind), a, b, c})) { setError("Control queue full. Try the action again."); return 0; }
    return 1;
}

// Renders frames (<= ms_max_frames) into ms_left()/ms_right() and refreshes telemetry.
MS_EXPORT int ms_process(int frames) {
    auto& s = state();
    if (frames < 0 || static_cast<std::size_t>(frames) > kMaxFrames) return 0;
    if (!s.prepared) { std::fill_n(s.left.data(), frames, 0.f); std::fill_n(s.right.data(), frames, 0.f); return 1; }
    s.session->process(s.left.data(), s.right.data(), static_cast<std::size_t>(frames));
    s.eventCount=0;ChangeEvent e;
    while(s.session->coreForOfflineTests().popChange(e) && s.eventCount<128){float* d=s.events.data()+s.eventCount++*3;d[0]=e.from;d[1]=e.to;d[2]=static_cast<unsigned>(e.reason);}
    auto& t = s.session->telemetry; auto& o = s.telemetry;
    o[MS_T_PASSES] = static_cast<float>(t.passes.load(std::memory_order_relaxed));
    o[MS_T_CURSOR] = static_cast<float>(t.cursor.load(std::memory_order_relaxed));
    o[MS_T_ROOM] = static_cast<float>(t.room.load(std::memory_order_relaxed));
    o[MS_T_ENVELOPE] = t.envelope.load(std::memory_order_relaxed);
    o[MS_T_THRESHOLD] = t.threshold.load(std::memory_order_relaxed);
    o[MS_T_PEAKL] = t.peakL.load(std::memory_order_relaxed); o[MS_T_PEAKR] = t.peakR.load(std::memory_order_relaxed);
    o[MS_T_GRAIN] = t.grainGain.load(std::memory_order_relaxed);
    o[MS_T_PLAYING] = t.playing.load(std::memory_order_relaxed) ? 1.f : 0.f;
    o[MS_T_INSTANCES] = static_cast<float>(t.instances.load(std::memory_order_relaxed));
    o[MS_T_FRAMES] = static_cast<float>(s.frames); o[MS_T_RATE] = s.rate; o[MS_T_LOOPFRAMES] = static_cast<float>(s.loopFrames);
    for (int i = 0; i < 8; ++i) {
        auto& v = t.voices[i]; float* d = o.data() + MS_T_VOICES + i*kVoiceFloats;
        d[0] = static_cast<float>(v.instance.load(std::memory_order_relaxed)); d[1] = static_cast<float>(v.room.load(std::memory_order_relaxed));
        d[2] = static_cast<float>(v.age.load(std::memory_order_relaxed)); d[3] = v.gain.load(std::memory_order_relaxed);
        d[4] = v.level.load(std::memory_order_relaxed); d[5] = v.active.load(std::memory_order_relaxed) ? 1.f : 0.f;
        d[6] = v.pending.load(std::memory_order_relaxed) ? 1.f : 0.f;
    }
    return 1;
}
MS_EXPORT void ms_destroy() { auto& s = state(); s.session.reset(); s.prepared = false; }
}
