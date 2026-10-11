#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace manyspace {
struct Room {
    float distance, speed, damping; bool swell;
    float lowpassHz = 6000, highpassHz = 20;
    float spread = 1;
    // Neutral defaults preserve existing descriptors and saved scenes.
    float diffusion = 0, predelay = 0, reflections = 0;
    float modulationDepth = 0, modulationRate = .3f;
    float shimmer = 0, hold = 0, drive = 1, ducking = 0, pan = 0, bloom = 0;
    bool validCharacter() const noexcept {
        const float values[] = {diffusion,predelay,reflections,modulationDepth,modulationRate,shimmer,hold,drive,ducking,pan,bloom};
        for(float x:values)if(!std::isfinite(x))return false;
        return diffusion>=0 && diffusion<=1 && predelay>=0 && predelay<=.12f
            && reflections>=0 && reflections<=7 && reflections==std::floor(reflections)
            && modulationDepth>=0 && modulationDepth<=.006f && modulationRate>=.1f && modulationRate<=1.5f
            && shimmer>=0 && shimmer<=.4f && hold>=0 && hold<=1 && drive>=1 && drive<=6
            && ducking>=0 && ducking<=.8f && pan>=-1 && pan<=1 && bloom>=0 && bloom<=.6f;
    }
};
constexpr std::size_t maxRooms=256;
struct Galaxy {
    std::size_t count{};
    std::array<Room, maxRooms> rooms{};
    std::array<std::array<float, maxRooms>, maxRooms> probabilities{};
};
struct Config {
    float departureSeconds = .005f;
    float arrivalSeconds = .010f;
    float arrivalFadeSeconds = .005f;
    float tailRampSeconds = .020f;
    float cooldownSeconds = .035f;
    float rearmSeconds = .012f;
};
struct Ramp {
    float value{}, target{}, delta{};
    std::uint32_t remaining{};
    void to(float t, std::uint32_t samples) noexcept {
        target = t; remaining = std::max(1u, samples); delta = (t - value) / remaining;
    }
    float tick() noexcept {
        if (remaining && !--remaining) value = target;
        else if (remaining) value += delta;
        return value;
    }
};
struct Diagnostics {
    std::uint64_t instance{};
    std::size_t room{};
    unsigned age{};
    float gain{}, target{}, level{};
    bool active{}, pending{};
};
enum class ChangeReason { Manual, Auto, Loop, Hand, Path };
struct ChangeEvent { unsigned from{},to{}; ChangeReason reason{}; };
enum class Trigger { Manual, Auto, Loop };
enum class Traversal { Next, Leap };

// Host stages controls/actions onto its audio thread. init is off-thread and
// copies a validated snapshot. process/jump/setKnobs allocate no memory.
class Core {
    struct Filter {
        double b0{}, b1{}, b2{}, a1{}, a2{};
        static Filter make(double hz, double sr, bool highpass) {
            const double k = std::tan(3.141592653589793 * std::min(hz, .45*sr)/sr);
            const double norm = 1/(1+std::sqrt(2.0)*k+k*k);
            const double b = (highpass ? 1:k*k)*norm;
            return {b, (highpass ? -2:2)*b, b, 2*(k*k-1)*norm, (1-std::sqrt(2.0)*k+k*k)*norm};
        }
    };
    struct FilterState {
        double z1{}, z2{};
        float tick(float input, const Filter& c) noexcept {
            const double output = c.b0*input+z1;
            z1 = c.b1*input-c.a1*output+z2;
            z2 = c.b2*input-c.a2*output;
            return static_cast<float>(output);
        }
    };
    struct RoomFilters { Filter input, highcut, lowcut; };
    struct GrainState { float energyA{}, energyB{}, cross{}, gain = 1; };
    struct Allpass {
        std::vector<float> data;
        std::vector<std::uint64_t> stamps;
        std::size_t write{};
        float tick(float input,float coefficient,std::uint64_t epoch) noexcept {
            float delayed=stamps[write]==epoch ? data[write]:0;
            float output=delayed-coefficient*input;
            data[write]=input+coefficient*output;stamps[write]=epoch;
            write=(write+1)%data.size();return output;
        }
    };
    struct Voice {
        std::vector<float> left, right;
        std::vector<std::uint64_t> stamps;
        std::uint64_t instance{};
        std::size_t room{}, write{}, pendingRoom{};
        unsigned age{};
        std::uint32_t arrival{}, replacement{};
        bool active{}, pending{};
        Ramp gain, input;
        Ramp bloom;
        std::array<Allpass,4> allpass;
        float modulationPhase{};
        float lpL{}, lpR{}, phase{}, level{};
        GrainState grainL, grainR;
        GrainState shimmerL, shimmerR;
        FilterState inputL, inputR, highcutL, highcutR, lowcutL, lowcutR;
    };
    Galaxy galaxy_{};
    std::array<RoomFilters,maxRooms> filters_{};
    Config config_{};
    std::array<Voice, 8> voices_{};
    std::size_t budget_{}, current_{}, route_{}, length_{};
    float sr_{}, mix_ = .4f, mixTarget_ = .4f, decay_ = 2.f, sensitivity_ = .5f;
    float decayTarget_ = 2.f, sensitivityTarget_ = .5f;
    float follower_{}, attack_{}, release_{}, smooth_{}, levelRelease_{};
    float energySmooth_{}, grainSmooth_{}, decaySmooth_{};
    float duckFollower_{}, duckRelease_{};
    std::array<ChangeEvent,128> changes_{};
    unsigned changeRead_{},changeWrite_{},droppedChanges_{};
    Trigger trigger_ = Trigger::Manual;
    bool armed_ = true;
    Traversal traversal_ = Traversal::Leap;
    std::uint32_t cooldown_{}, below_{};
    std::uint64_t serial_{}, rng_{};

    std::uint32_t samples(float seconds) const noexcept {
        return std::max(1u, static_cast<std::uint32_t>(std::ceil(seconds * sr_)));
    }
    float random() noexcept {
        rng_ ^= rng_ >> 12; rng_ ^= rng_ << 25; rng_ ^= rng_ >> 27;
        return static_cast<float>((rng_ * UINT64_C(2685821657736338717)) >> 40) / 16777216.f;
    }
    void activate(Voice& v, std::size_t room) noexcept {
        v.active = true; v.pending = false; v.room = room; v.age = 0;
        v.instance = ++serial_; v.write = 0; v.phase = 0; v.lpL = v.lpR = v.level = 0;
        v.grainL = {}; v.grainR = {};
        v.shimmerL={};v.shimmerR={};
        v.modulationPhase=0;
        for(auto& a:v.allpass)a.write=0;
        v.inputL = {}; v.inputR = {}; v.highcutL = {}; v.highcutR = {}; v.lowcutL = {}; v.lowcutR = {};
        v.gain = {}; v.gain.value = v.gain.target = 1;
        v.input = {}; v.arrival = samples(config_.arrivalSeconds);
        v.bloom={};v.bloom.value=v.bloom.target=1;
        if(galaxy_.rooms[room].bloom>0) {
            // Capture the initiating attack; bloom shapes output, never capture.
            v.input.value=v.input.target=1;v.arrival=0;
            v.bloom.value=0;v.bloom.to(1,samples(galaxy_.rooms[room].bloom));
        }
        v.replacement = 0;
        // Epoch stamps make old samples zero without clearing buffers in process.
    }
    float read(const Voice& v, const std::vector<float>& buffer, float delay) const noexcept {
        // Keep wrapping in double: a tiny negative float offset can round up
        // to length when adding the ring size, indexing one past the buffer.
        double position = static_cast<double>(v.write) - static_cast<double>(delay);
        if (position < 0) position += static_cast<double>(length_);
        auto a = static_cast<std::size_t>(position);
        auto b = (a + 1) % length_;
        float f = static_cast<float>(position - static_cast<double>(a));
        float va = v.stamps[a] == v.instance ? buffer[a] : 0;
        float vb = v.stamps[b] == v.instance ? buffer[b] : 0;
        return va + f * (vb - va);
    }
    float grain(const Voice& v, const std::vector<float>& buffer, float base, float speed, GrainState& state) const noexcept {
        if (std::abs(speed - 1.f) < 1e-6f) return read(v, buffer, base);
        float p = v.phase, p2 = p < .5f ? p + .5f : p - .5f;
        // Complementary raised-cosine windows have zero slope at grain wraps.
        float w = .5f - .5f * std::cos(6.283185307f * p);
        float a = read(v, buffer, base + p * .080f * sr_);
        float b = read(v, buffer, base + p2 * .080f * sr_);
        state.energyA = energySmooth_ * state.energyA + (1 - energySmooth_) * a * a;
        state.energyB = energySmooth_ * state.energyB + (1 - energySmooth_) * b * b;
        state.cross = energySmooth_ * state.cross + (1 - energySmooth_) * a * b;
        float target = w * state.energyA + (1 - w) * state.energyB;
        float actual = w*w * state.energyA + (1-w)*(1-w) * state.energyB + 2*w*(1-w)*state.cross;
        float desired = target > 1e-10f ? std::clamp(std::sqrt(target / std::max({actual, .01f * target, 1e-12f})), 1.f, 3.f) : 1.f;
        state.gain = desired + grainSmooth_ * (state.gain - desired);
        return state.gain * (w * a + (1 - w) * b);
    }
public:
    static float ageTarget(unsigned age) noexcept {
        if (!age) return 1;
        float db = -6.f - std::min(6u, age - 1) * (74.f / 6.f);
        return std::pow(10.f, db / 20.f);
    }
    void init(const Galaxy& g, float sampleRate, std::size_t budget,
              std::uint64_t playbackSeed = 11, Config config = {}) {
        if ((budget != 4 && budget != 8) || !std::isfinite(sampleRate)
            || sampleRate < 8000 || sampleRate > 96000 || (g.count != 16 && g.count != 64 && g.count != 256))
            throw std::invalid_argument("rate, budget or room count");
        const float times[] = {config.departureSeconds, config.arrivalSeconds,
            config.arrivalFadeSeconds, config.tailRampSeconds, config.cooldownSeconds, config.rearmSeconds};
        for (float t : times) if (!std::isfinite(t) || t < .0001f || t > 1)
            throw std::invalid_argument("configuration time");
        for (std::size_t i = 0; i < g.count; ++i) {
            const auto& r = g.rooms[i];
            if (!std::isfinite(r.distance) || r.distance < .015f || r.distance > .09501f
                || !std::isfinite(r.speed) || r.speed < .5f || r.speed > 1.5f
                || !std::isfinite(r.damping) || r.damping < .2f || r.damping > .85001f
                || !std::isfinite(r.lowpassHz) || r.lowpassHz < 800 || r.lowpassHz > 16000
                || !std::isfinite(r.highpassHz) || r.highpassHz < 10 || r.highpassHz > 1000
                || !std::isfinite(r.spread) || r.spread < 0 || r.spread > 1
                || !r.validCharacter()
                || r.highpassHz >= r.lowpassHz)
                throw std::invalid_argument("room range");
            float sum = 0;
            for (std::size_t j = 0; j < g.count; ++j) {
                float p = g.probabilities[i][j];
                if (!std::isfinite(p) || p < 0 || p > 1) throw std::invalid_argument("probability");
                sum += p;
            }
            if (std::abs(sum - 1.f) > 1e-5f) throw std::invalid_argument("row normalization");
        }
        galaxy_ = g; sr_ = sampleRate; budget_ = budget; config_ = config;
        for (std::size_t i = 0; i < g.count; ++i) {
            const auto& room = g.rooms[i];
            // Before varispeed reads, limit bandwidth more when speeding up.
            // These gentle filters reduce folding; they are not a brick-wall resampler.
            const float ratio = std::max(1.f,room.speed);
            filters_[i] = {Filter::make(std::min(room.lowpassHz,.45f*sr_)/ratio,sr_,false),
                Filter::make(room.lowpassHz,sr_,false), Filter::make(room.highpassHz,sr_,true)};
        }
        bool extended=false,diffused=false;
        for(std::size_t i=0;i<g.count;++i){extended=extended || g.rooms[i].predelay>0;diffused=diffused || g.rooms[i].diffusion>0;}
        length_ = static_cast<std::size_t>(std::ceil(sr_ * (extended ? .320f:.200f))) + 4;
        serial_ = 0; rng_ = playbackSeed ? playbackSeed : UINT64_C(0x9e3779b97f4a7c15);
        changeRead_=changeWrite_=droppedChanges_=0;
        follower_ = 0; cooldown_ = below_ = 0; armed_ = true; trigger_ = Trigger::Manual; traversal_ = Traversal::Leap;
        mix_ = mixTarget_ = .4f; decay_ = decayTarget_ = 2; sensitivity_ = sensitivityTarget_ = .5f;
        attack_ = std::exp(-1.f / (.001f * sr_)); release_ = std::exp(-1.f / (.025f * sr_));
        smooth_ = std::exp(-1.f / (.010f * sr_)); levelRelease_ = std::exp(-1.f / (.050f * sr_));
        energySmooth_ = decaySmooth_ = std::exp(-1.f / (.020f * sr_));
        grainSmooth_ = std::exp(-1.f / (.001f * sr_));
        duckFollower_=0;duckRelease_=std::exp(-1.f/(.22f*sr_));
        for (std::size_t i = 0; i < 8; ++i) {
            voices_[i] = Voice{};
            if (i < budget_) {
                voices_[i].left.assign(length_, 0); voices_[i].right.assign(length_, 0);
                voices_[i].stamps.assign(length_, 0);
                if(diffused) {
                    const float times[]={.0047f,.0113f,.0053f,.0127f};
                    for(unsigned j=0;j<4;++j) {
                        auto n=samples(times[j]);voices_[i].allpass[j].data.assign(n,0);voices_[i].allpass[j].stamps.assign(n,0);
                    }
                }
            }
        }
        current_ = route_ = 0; activate(voices_[0], 0);
    }
    void setKnobs(float mix, float decay, float sensitivity) noexcept {
        if (std::isfinite(mix)) mixTarget_ = std::clamp(mix, 0.f, 1.f);
        if (std::isfinite(decay)) decayTarget_ = std::clamp(decay, .3f, 8.f);
        if (std::isfinite(sensitivity)) sensitivityTarget_ = std::clamp(sensitivity, 0.f, 1.f);
    }
    void setTrigger(Trigger mode) noexcept { trigger_ = mode; }
    void setAutomatic(bool enabled) noexcept { setTrigger(enabled ? Trigger::Auto : Trigger::Manual); }
    Trigger trigger() const noexcept { return trigger_; }
    void traverse(ChangeReason reason=ChangeReason::Manual) noexcept { if (traversal_ == Traversal::Next) next(reason); else leap(reason); }
    bool popChange(ChangeEvent& e) noexcept { if(changeRead_==changeWrite_)return false;e=changes_[changeRead_];changeRead_=(changeRead_+1)%changes_.size();return true; }
    unsigned droppedChanges() const noexcept { return droppedChanges_; }
    void setTraversal(Traversal mode) noexcept { traversal_ = mode; }
    float threshold() const noexcept { return .5f * std::pow(.02f, sensitivity_); }
    float envelope() const noexcept { return follower_; }
    std::size_t currentRoom() const noexcept { return current_; }
    std::uint64_t createdInstances() const noexcept { return serial_; }
    float maximumGrainGain() const noexcept {
        float gain = 1;
        for (std::size_t i = 0; i < budget_; ++i) gain = std::max({gain, voices_[i].grainL.gain, voices_[i].grainR.gain});
        return gain;
    }
    std::size_t memoryBytes() const noexcept {
        std::size_t bytes=budget_*length_*(2*sizeof(float)+sizeof(std::uint64_t));
        for(std::size_t i=0;i<budget_;++i)for(const auto& a:voices_[i].allpass)bytes+=a.data.size()*(sizeof(float)+sizeof(std::uint64_t));
        return bytes;
    }
    Diagnostics diagnostic(std::size_t i) const noexcept {
        const auto& v = voices_[i];
        return {v.instance, v.room, v.age, v.gain.value, v.gain.target, v.level, v.active, v.pending};
    }
    void jump(std::size_t destination, ChangeReason reason=ChangeReason::Manual) noexcept {
        if (!budget_ || destination >= galaxy_.count) return;
        const auto w=(changeWrite_+1)%changes_.size();
        if(w!=changeRead_){changes_[changeWrite_]={static_cast<unsigned>(current_),static_cast<unsigned>(destination),reason};changeWrite_=w;}else ++droppedChanges_;
        current_ = destination;
        // Pending destinations can be superseded; fade countdown is not restarted.
        std::size_t slot = budget_;
        for (std::size_t i = 0; i < budget_; ++i) {
            auto& v = voices_[i];
            if (v.pending) { slot = i; v.pendingRoom = destination; }
            if (v.active) {
                v.age = std::min(v.age + 1, 1000000u);
                // A departing bloom cannot rise against its age attenuation.
                v.bloom.to(v.bloom.value,1);
                if (!v.pending) v.gain.to(ageTarget(v.age), samples(config_.tailRampSeconds));
                v.input.to(0, samples(config_.departureSeconds));
                v.arrival = 0;
            }
        }
        if (slot != budget_) { route_ = slot; return; }
        for (std::size_t i = 0; i < budget_; ++i) if (!voices_[i].active) { slot = i; break; }
        if (slot != budget_) activate(voices_[slot], destination);
        else {
            // Quietest weighted tail, deterministic tie break. Fade before epoch reuse.
            slot = 0;
            for (std::size_t i = 1; i < budget_; ++i)
                if (voices_[i].level * voices_[i].gain.value < voices_[slot].level * voices_[slot].gain.value) slot = i;
            auto& v = voices_[slot];
            v.pending = true; v.pendingRoom = destination;
            v.replacement = samples(config_.departureSeconds);
            v.gain.to(0, v.replacement); v.input.to(0, v.replacement);
        }
        route_ = slot;
    }
    void next(ChangeReason reason=ChangeReason::Manual) noexcept { if (galaxy_.count) jump((current_ + 1) % galaxy_.count,reason); }
    void leap(ChangeReason reason=ChangeReason::Manual) noexcept {
        if (!galaxy_.count) return;
        float draw = random(), total = 0;
        for (std::size_t i = 0; i < galaxy_.count; ++i) {
            total += galaxy_.probabilities[current_][i];
            if (draw < total || i + 1 == galaxy_.count) { jump(i,reason); break; }
        }
    }
    void process(const float* left, const float* right, float* outL, float* outR, std::size_t frames) noexcept {
        for (std::size_t s = 0; s < frames; ++s) {
            decay_ = decayTarget_ + decaySmooth_ * (decay_ - decayTarget_);
            sensitivity_ = sensitivityTarget_ + smooth_ * (sensitivity_ - sensitivityTarget_);
            float l = std::isfinite(left[s]) ? left[s] : 0;
            float r = std::isfinite(right[s]) ? right[s] : 0;
            float dry = std::max(std::abs(l), std::abs(r));
            float c = dry > follower_ ? attack_ : release_;
            follower_ = dry + c * (follower_ - dry);
            duckFollower_=dry+(dry>duckFollower_ ? attack_:duckRelease_)*(duckFollower_-dry);
            if (cooldown_) --cooldown_;
            if (follower_ < threshold() * .5f) {
                if (++below_ >= samples(config_.rearmSeconds)) armed_ = true;
            } else below_ = 0;
            if (trigger_ == Trigger::Auto && armed_ && !cooldown_ && follower_ >= threshold()) {
                armed_ = false; cooldown_ = samples(config_.cooldownSeconds);
                traverse(ChangeReason::Auto);
            }
            float wetL = 0, wetR = 0;
            for (std::size_t i = 0; i < budget_; ++i) {
                auto& v = voices_[i];
                if (!v.active) continue;
                const auto& room = galaxy_.rooms[v.room];
                const auto& filters = filters_[v.room];
                if (v.arrival && !--v.arrival && i == route_ && !v.age && !v.pending)
                    v.input.to(1, samples(config_.arrivalFadeSeconds));
                float input = v.input.tick(), gain = v.gain.tick()*v.bloom.tick();
                float delay = room.distance * sr_;
                float modulation=0;
                if(room.modulationDepth>0) {
                    const float depth=std::min(room.modulationDepth,room.speed<.8f ? .0025f:.006f);
                    modulation=depth*sr_*std::sin(v.modulationPhase*6.283185307f);
                    v.modulationPhase+=room.modulationRate/sr_;
                    if(v.modulationPhase>=1)v.modulationPhase-=1;
                }
                float tapL = read(v, v.left, delay+modulation), tapR = read(v, v.right, delay * 1.037f-modulation);
                if(room.diffusion>0) {
                    const float coefficient=.7f*room.diffusion;
                    tapL=v.allpass[1].tick(v.allpass[0].tick(tapL,coefficient,v.instance),coefficient,v.instance);
                    tapR=v.allpass[3].tick(v.allpass[2].tick(tapR,coefficient,v.instance),coefficient,v.instance);
                }
                float cutoff = 1200.f + (1.f - room.damping*(1-room.hold)) * 9000.f;
                float lp = std::exp(-6.283185307f * cutoff / sr_);
                v.lpL = (1.f - lp) * tapL + lp * v.lpL;
                v.lpR = (1.f - lp) * tapR + lp * v.lpR;
                float feedback = std::min(.98f, std::pow(.001f, room.distance / decay_));
                if(room.hold>0)feedback+=(.999f-feedback)*room.hold;
                float speed = room.swell ? -room.speed : room.speed;
                float a = grain(v, v.left, delay+room.predelay*sr_, speed, v.grainL), b = grain(v, v.right, delay * 1.037f+room.predelay*sr_, speed, v.grainR);
                // Shimmer regenerates from a separate, un-delayed output reader;
                // pre-delay, placement and ducking never enter its feedback path.
                float regenerationL=0,regenerationR=0;
                if(room.shimmer>0 && std::abs(speed-1)>1e-6f) {
                    regenerationL=grain(v,v.left,delay,speed,v.shimmerL)/v.shimmerL.gain;
                    regenerationR=grain(v,v.right,delay*1.037f,speed,v.shimmerR)/v.shimmerR.gain;
                }
                if(room.reflections>0) {
                    static constexpr float patterns[7][4]={{.31f,.57f,.79f,1.23f},{.24f,.48f,.72f,.96f},{.63f,.69f,.76f,.87f},
                        {.19f,.43f,.81f,1.17f},{.37f,.41f,.93f,1.19f},{.51f,.71f,1.01f,1.27f},{.23f,.61f,.89f,1.11f}};
                    const auto& pattern=patterns[static_cast<unsigned>(room.reflections)-1];
                    float earlyL=0,earlyR=0;
                    for(unsigned t=0;t<4;++t) {
                        float offset=delay*pattern[t]+room.predelay*sr_;
                        earlyL+=(t%2 ? .08f:.16f)*read(v,v.left,offset);
                        earlyR+=(t%2 ? .16f:.08f)*read(v,v.right,offset*1.003f);
                    }
                    a=.7f*a+earlyL;b=.7f*b+earlyR;
                }
                a = v.lowcutL.tick(v.highcutL.tick(a,filters.highcut),filters.lowcut);
                b = v.lowcutR.tick(v.highcutR.tick(b,filters.highcut),filters.lowcut);
                // Width belongs to this visit, including its departing tail.
                // Narrow only the wet side signal; unity retains the original
                // stereo samples exactly. Keep feedback histories unchanged.
                if (room.spread < 1) {
                    float mid = .5f * (a + b), side = .5f * (a - b) * room.spread;
                    a = mid + side; b = mid - side;
                }
                if(room.pan!=0) {
                    a*=std::sqrt(1-room.pan);b*=std::sqrt(1+room.pan);
                }
                if(room.ducking>0)gain*=1-room.ducking*std::clamp(duckFollower_*4,0.f,1.f);
                // Input/feedback are softly bounded internally; dry path is exact.
                float loopL=v.lpL,loopR=v.lpR;
                if(room.shimmer>0 && std::abs(speed-1)>1e-6f) {
                    loopL=(1-room.shimmer)*loopL+room.shimmer*regenerationL;
                    loopR=(1-room.shimmer)*loopR+room.shimmer*regenerationR;
                }
                const float writeL=input*l+feedback*loopL,writeR=input*r+feedback*loopR;
                v.left[v.write] = v.inputL.tick(room.drive==1 ? std::tanh(writeL):std::tanh(room.drive*writeL)/room.drive,filters.input);
                v.right[v.write] = v.inputR.tick(room.drive==1 ? std::tanh(writeR):std::tanh(room.drive*writeR)/room.drive,filters.input);
                v.stamps[v.write] = v.instance;
                v.write = (v.write + 1) % length_;
                v.phase += (1.f - speed) / (.080f * sr_);
                if (v.phase >= 1) v.phase -= 1;
                if (v.phase < 0) v.phase += 1;
                v.level = std::max(std::max(std::abs(a), std::abs(b)), v.level * levelRelease_);
                wetL += .18f * gain * a; wetR += .18f * gain * b;
                if (v.pending && v.replacement && !--v.replacement) activate(v, v.pendingRoom);
            }
            mix_ = mixTarget_ + smooth_ * (mix_ - mixTarget_);
            if (std::abs(mix_-mixTarget_) < 1e-7f) mix_ = mixTarget_;
            outL[s] = l + mix_ * std::tanh(wetL);
            outR[s] = r + mix_ * std::tanh(wetR);
        }
    }
};
} // namespace manyspace
