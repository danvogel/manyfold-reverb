#pragma once
#include "listening.hpp"
#include <atomic>
#include <limits>

namespace manyspace {
enum class CommandKind { Knobs, Next, Leap, Room, Automatic, TraversalMode, Play, Loop, Bus };
enum class AuditionBus { Dry, Effect, Wet, Immersion };
struct Command { CommandKind kind{}; float a{}, b{}, c{}; };
template<std::size_t Capacity = 64> class CommandQueue {
    static_assert(Capacity > 1);
    std::array<Command, Capacity> commands_{};
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
public:
    bool push(Command command) noexcept {
        auto w = write_.load(std::memory_order_relaxed), next = (w+1)%Capacity;
        if (next == read_.load(std::memory_order_acquire)) return false;
        commands_[w] = command; write_.store(next,std::memory_order_release); return true;
    }
    bool pop(Command& command) noexcept {
        auto r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;
        command = commands_[r]; read_.store((r+1)%Capacity,std::memory_order_release); return true;
    }
    void resetStopped() noexcept { read_.store(0); write_.store(0); }
};
static_assert(std::atomic<float>::is_always_lock_free && std::atomic<std::uint64_t>::is_always_lock_free
              && std::atomic<std::size_t>::is_always_lock_free, "host requires lock-free telemetry");
struct VoiceTelemetry {
    std::atomic<std::uint64_t> instance{0};
    std::atomic<unsigned> room{0}, age{0};
    std::atomic<float> gain{0}, level{0};
    std::atomic<bool> active{false}, pending{false};
};
struct Telemetry {
    std::atomic<std::size_t> cursor{0}, room{0}, passes{0};
    std::atomic<float> envelope{0}, threshold{0}, peakL{0}, peakR{0}, grainGain{1};
    std::atomic<bool> playing{false};
    std::atomic<std::uint64_t> instances{0}, callbacks{0}, maxCallbackNanos{0}, totalCallbackNanos{0}, callbackFrames{0}, overruns{0};
    std::array<VoiceTelemetry,8> voices;
};

// prepare/clip access only while device callback is stopped. Queue is main->audio;
// telemetry fields are independent observations, not a coherent preset snapshot.
class Session {
    Core core_;
    Galaxy galaxy_{};
    Clip clip_;
    Scenario scenario_ = scenarios()[0];
    std::size_t cursor_{}, budget_ = 8, phraseFrames_{}, nextPhrase_{}, phraseIndex_{};
    bool wantedPlay_{}, loop_ = true, boundaryHandled_{};
    std::size_t passes_{};
    Ramp transport_, dry_, wet_;
    AuditionBus bus_ = AuditionBus::Effect;
    float mix_ = .55f;
    std::array<float,1024> effectL_{}, effectR_{};
    std::uint32_t fadeFrames_{};
    std::size_t phraseBoundary(std::size_t index) const noexcept {
        return static_cast<std::size_t>(std::llround(index*8*60.0/clip_.bpm*clip_.sampleRate));
    }
    static float immersionDry(float mix) noexcept { return 1.f-mix; }
    static float immersionWet() noexcept { return 4.f; }
    static float peakGuard(float x) noexcept {
        const float magnitude=std::abs(x);
        return magnitude<=.9f ? x : std::copysign(.9f+.1f*std::tanh((magnitude-.9f)*10.f),x);
    }
    void commands() noexcept {
        Command c;
        // A finite upper bound even if a producer pushes during draining.
        for (unsigned i = 0; i < 63 && queue.pop(c); ++i) switch (c.kind) {
            case CommandKind::Knobs:
                core_.setKnobs(c.a,c.b,c.c); mix_=std::clamp(c.a,0.f,1.f);
                if(bus_==AuditionBus::Immersion) {
                    dry_.to(immersionDry(mix_),fadeFrames_);wet_.to(immersionWet(),fadeFrames_);
                }
                break;
            case CommandKind::Next: core_.next(); break;
            case CommandKind::Leap: core_.leap(); break;
            case CommandKind::Room: if (c.a >= 0 && c.a < galaxy_.count) core_.jump(static_cast<std::size_t>(c.a),ChangeReason::Hand); break;
            case CommandKind::Automatic: core_.setTrigger(static_cast<Trigger>(static_cast<int>(c.a))); break;
            case CommandKind::TraversalMode: core_.setTraversal(c.a == 0 ? Traversal::Next : Traversal::Leap); break;
            case CommandKind::Play: if (c.a && boundaryHandled_ && !loop_) { cursor_=0; boundaryHandled_=false; } wantedPlay_ = c.a != 0; transport_.to(wantedPlay_ ? 1.f : 0.f,fadeFrames_); break;
            case CommandKind::Loop: loop_ = c.a != 0; break;
            case CommandKind::Bus:
                bus_=static_cast<AuditionBus>(static_cast<int>(c.a));
                dry_.to(bus_==AuditionBus::Wet ? 0.f : bus_==AuditionBus::Immersion ? immersionDry(mix_) : 1.f,fadeFrames_);
                wet_.to(bus_==AuditionBus::Dry ? 0.f : bus_==AuditionBus::Immersion ? immersionWet() : 1.f,fadeFrames_); break;
        }
    }
public:
    CommandQueue<> queue;
    Telemetry telemetry;
    void prepare(Galaxy g, Clip clip, const Scenario& scenario, std::size_t budget = 8, std::uint64_t seed = 11) {
        if (clip.left.empty() || clip.left.size() != clip.right.size() || !std::isfinite(clip.sampleRate)
            || clip.sampleRate < 8000 || clip.sampleRate > 96000 || !std::isfinite(clip.bpm) || clip.bpm < 0
            || clip.loopFrames > clip.left.size())
            throw std::invalid_argument("invalid listening clip");
        float peak = 1;
        for (auto* data : {&clip.left,&clip.right}) for (auto& x : *data) {
            if (!std::isfinite(x)) x = 0;
            peak = std::max(peak,std::abs(x));
        }
        for (auto* data : {&clip.left,&clip.right}) for (auto& x : *data) x /= peak;
        galaxy_ = auditionGalaxy(g,scenario.treatment); clip_ = std::move(clip); scenario_ = scenario; budget_ = budget;
        core_.init(galaxy_,clip_.sampleRate,budget_,seed);
        core_.setKnobs(scenario.mix,scenario.decay,scenario.sensitivity);
        core_.setTraversal(scenario.traversal); core_.setTrigger(scenario.trigger == Trigger::Manual && scenario.automatic ? Trigger::Auto : scenario.trigger);
        std::array<float,1024> zero{}, a{}, b{};
        for (std::size_t i = 0, n = static_cast<std::size_t>(clip_.sampleRate*.25f); i < n;) {
            auto size = std::min(n-i,zero.size()); core_.process(zero.data(),zero.data(),a.data(),b.data(),size); i += size;
        }
        cursor_ = 0; passes_ = 0; boundaryHandled_ = false; wantedPlay_ = false; loop_ = true;
        transport_ = {}; dry_ = {}; wet_ = {}; bus_=AuditionBus::Effect; mix_=std::clamp(scenario.mix,0.f,1.f);
        dry_.value = dry_.target = wet_.value = wet_.target = 1;
        fadeFrames_ = static_cast<std::uint32_t>(std::ceil(clip_.sampleRate*.005f));
        phraseFrames_ = scenario.phraseNext && clip_.bpm > 0 ? static_cast<std::size_t>(std::llround(8*60.0/clip_.bpm*clip_.sampleRate)) : 0;
        phraseIndex_ = 1; nextPhrase_ = phraseFrames_;
        queue.resetStopped();
        telemetry.callbacks = 0; telemetry.maxCallbackNanos = 0; telemetry.totalCallbackNanos = 0;
        telemetry.callbackFrames = 0; telemetry.overruns = 0;
        publish(0,0);
    }
    const Clip& clipStopped() const { return clip_; }
    const Galaxy& galaxyStopped() const { return galaxy_; }
    Core& coreForOfflineTests() { return core_; } // Never use from UI while running.
    void publish(float peakL, float peakR) noexcept {
        telemetry.cursor.store(cursor_,std::memory_order_relaxed);
        telemetry.passes.store(passes_,std::memory_order_relaxed);
        telemetry.room.store(core_.currentRoom(),std::memory_order_relaxed);
        telemetry.envelope.store(core_.envelope(),std::memory_order_relaxed);
        telemetry.threshold.store(core_.threshold(),std::memory_order_relaxed);
        telemetry.peakL.store(peakL,std::memory_order_relaxed); telemetry.peakR.store(peakR,std::memory_order_relaxed);
        telemetry.grainGain.store(core_.maximumGrainGain(),std::memory_order_relaxed);
        telemetry.instances.store(core_.createdInstances(),std::memory_order_relaxed);
        telemetry.playing.store(wantedPlay_,std::memory_order_relaxed);
        for (std::size_t i = 0; i < 8; ++i) {
            auto d = core_.diagnostic(i); auto& t = telemetry.voices[i];
            t.room.store(static_cast<unsigned>(d.room),std::memory_order_relaxed);
            t.age.store(d.age,std::memory_order_relaxed); t.gain.store(d.gain,std::memory_order_relaxed);
            t.level.store(d.level,std::memory_order_relaxed); t.active.store(d.active,std::memory_order_relaxed);
            t.pending.store(d.pending,std::memory_order_relaxed); t.instance.store(d.instance,std::memory_order_relaxed);
        }
    }
    void process(float* left, float* right, std::size_t frames) noexcept {
        commands();
        float peakL = 0, peakR = 0;
        std::size_t done = 0;
        while (done < frames) {
            if (clip_.left.empty() || (!wantedPlay_ && transport_.value == 0 && !transport_.remaining)) {
                std::fill(left+done,left+frames,0); std::fill(right+done,right+frames,0); break;
            }
            auto end = loop_ && clip_.loopFrames ? clip_.loopFrames:clip_.left.size();
            if (cursor_ >= end) {
                if (!boundaryHandled_) { ++passes_; if (core_.trigger()==Trigger::Loop && !phraseFrames_) core_.traverse(ChangeReason::Loop); boundaryHandled_=true; }
                if (!loop_) {
                    wantedPlay_ = false; transport_ = {};
                    std::fill(left+done,left+frames,0); std::fill(right+done,right+frames,0); break;
                }
                cursor_ = 0; boundaryHandled_=false; phraseIndex_ = 1; nextPhrase_ = phraseFrames_;
                // The next loop's first beat continues Path's two-bar schedule.
                if (phraseFrames_ && end >= phraseFrames_) core_.next(ChangeReason::Path);
            }
            const auto musicEnd = clip_.loopFrames ? clip_.loopFrames:static_cast<std::size_t>(std::llround(clip_.musicSeconds*clip_.sampleRate));
            if (phraseFrames_ && cursor_ >= nextPhrase_ && nextPhrase_ < musicEnd) {
                core_.next(ChangeReason::Path); nextPhrase_ = phraseBoundary(++phraseIndex_);
            }
            auto size = std::min({effectL_.size(), frames-done,end-cursor_});
            if (phraseFrames_ && nextPhrase_ > cursor_) size = std::min(size,nextPhrase_-cursor_);
            if (!wantedPlay_ && transport_.remaining) size = std::min(size,static_cast<std::size_t>(transport_.remaining));
            core_.process(clip_.left.data()+cursor_,clip_.right.data()+cursor_,effectL_.data(),effectR_.data(),size);
            for (std::size_t i = 0; i < size; ++i) {
                float dry = dry_.tick(), wet = wet_.tick(), transport = transport_.tick();
                // Same fixed headroom on all audition buses. No output knob.
                left[done+i] = .5f*transport*(dry*clip_.left[cursor_+i]+wet*(effectL_[i]-clip_.left[cursor_+i]));
                right[done+i] = .5f*transport*(dry*clip_.right[cursor_+i]+wet*(effectR_[i]-clip_.right[cursor_+i]));
                // New notebook gain can exceed unity; leave ordinary levels untouched.
                if(bus_==AuditionBus::Immersion || wet>1.f) {
                    left[done+i]=peakGuard(left[done+i]);right[done+i]=peakGuard(right[done+i]);
                }
                peakL = std::max(peakL,std::abs(left[done+i])); peakR = std::max(peakR,std::abs(right[done+i]));
            }
            cursor_ += size; done += size;
            if (cursor_ == end && !boundaryHandled_) {
                ++passes_; if (core_.trigger()==Trigger::Loop && !phraseFrames_) core_.traverse(ChangeReason::Loop); boundaryHandled_=true;
                if (!loop_) { wantedPlay_=false; transport_={}; }
            }
        }
        publish(peakL,peakR);
    }
};
} // namespace manyspace
