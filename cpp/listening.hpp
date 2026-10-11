#pragma once
#include "core.hpp"
#include <string>

namespace manyspace {
enum class Example { Plucks, Drums, Chords };
enum class Treatment { Galaxy, Normal, Reverse, Half };
struct Clip {
    std::vector<float> left, right;
    float sampleRate{}, bpm{}, musicSeconds{};
    std::size_t loopFrames{}; // Musical/source boundary; appended tail padding is one-shot only.
    std::string name;
};
struct Scenario {
    const char* id;
    const char* name;
    const char* purpose;
    Example example;
    Treatment treatment;
    float mix, decay, sensitivity;
    bool automatic;
    Traversal traversal;
    bool phraseNext;
    Trigger trigger = Trigger::Manual; // automatic remains a compatibility field for the Studio host.
};
inline const std::array<Scenario, 9>& scenarios() {
    static const std::array<Scenario, 9> s{{
        {"plucks-close", "Plucks · close reflections", "Short decay leaves space for the next note.", Example::Plucks, Treatment::Normal, .28f, .7f, .65f, false, Traversal::Next, false},
        {"plucks-space", "Plucks · longer space", "Same phrase and room; longer tails connect the rests.", Example::Plucks, Treatment::Normal, .55f, 2.8f, .65f, false, Traversal::Next, false},
        {"plucks-galaxy", "Plucks · phrase traversal", "Next changes room every two bars; departing tails remain.", Example::Plucks, Treatment::Galaxy, .55f, 2.8f, .65f, false, Traversal::Next, true},
        {"drums-accents", "Drums · accent jumps", "Lower sensitivity favors strong accents. Auto Next.", Example::Drums, Treatment::Galaxy, .45f, .9f, .35f, true, Traversal::Next, false},
        {"drums-details", "Drums · detail jumps", "Higher sensitivity admits quieter hits. Auto Leap.", Example::Drums, Treatment::Galaxy, .45f, 1.6f, .90f, true, Traversal::Leap, false},
        {"chords-normal", "Chords · forward", "Normal-speed room for the matched treatment comparison.", Example::Chords, Treatment::Normal, .55f, 4.8f, .65f, false, Traversal::Next, false},
        {"chords-reverse", "Chords · reverse grains", "Same input, spacing and damping; reverse-grain texture.", Example::Chords, Treatment::Reverse, .55f, 4.8f, .65f, false, Traversal::Next, false},
        {"chords-half", "Chords · half speed", "Same input; octave-down grains add a lower layer.", Example::Chords, Treatment::Half, .55f, 4.8f, .65f, false, Traversal::Next, false},
        {"plucks-reverse", "Plucks · reverse grains", "Compare reverse level and transients against longer space.", Example::Plucks, Treatment::Reverse, .55f, 2.8f, .65f, false, Traversal::Next, false}
    }};
    return s;
}
struct Noise {
    std::uint64_t state = 42;
    float next() {
        state ^= state >> 12; state ^= state << 25; state ^= state >> 27;
        return static_cast<float>((state * UINT64_C(2685821657736338717)) >> 40) / 8388608.f - 1.f;
    }
};
inline float noteHz(int midi) { return 440.f * std::pow(2.f, (midi - 69) / 12.f); }
inline Clip generateClip(Example example, float sr) {
    Clip clip;
    clip.sampleRate = sr;
    clip.bpm = example == Example::Plucks ? 96 : example == Example::Drums ? 110 : 72;
    clip.name = example == Example::Plucks ? "plucks" : example == Example::Drums ? "drums" : "chords";
    float beat = 60 / clip.bpm;
    clip.musicSeconds = 32 * beat;
    clip.loopFrames = static_cast<std::size_t>(std::llround(32*60.0/clip.bpm*sr));
    auto frames = static_cast<std::size_t>(std::ceil((clip.musicSeconds + 6) * sr));
    clip.left.assign(frames, 0); clip.right.assign(frames, 0);
    Noise noise;
    const int notes[4][4] = {{52,55,59,62}, {48,52,55,59}, {43,50,55,62}, {50,54,57,64}};
    auto add = [&](float when, float duration, auto synth) {
        auto start = static_cast<std::size_t>(when * sr);
        auto count = static_cast<std::size_t>(duration * sr);
        for (std::size_t j = 0; j < count && start + j < frames; ++j) {
            auto stereo = synth(j / sr, j);
            clip.left[start+j] += stereo[0]; clip.right[start+j] += stereo[1];
        }
    };
    if (example == Example::Plucks) {
        const int pattern[8] = {0,2,1,3,0,1,2,-1};
        for (int bar = 0; bar < 8; ++bar) for (int step = 0; step < 8; ++step) {
            if (pattern[step] < 0) continue;
            float hz = noteHz(notes[bar/2][pattern[step]]);
            std::vector<float> string(std::max(2u, static_cast<unsigned>(sr / hz)));
            for (auto& x : string) x = noise.next();
            std::size_t index = 0;
            float velocity = step == 0 ? 1 : step == 4 ? .8f : .42f;
            add((bar*4 + step*.5f) * beat, .26f, [&](float t, std::size_t) {
                float value = string[index];
                string[index] = .498f * (value + string[(index+1)%string.size()]);
                index = (index+1)%string.size();
                float release = std::min(1.f, (.26f-t)/.035f);
                float y = value * velocity * std::exp(-9*t) * release;
                return std::array<float,2>{y, y*.94f};
            });
        }
    } else if (example == Example::Drums) {
        auto kick = [&](float when, float level) {
            add(when, .28f, [&](float t, std::size_t) {
                float phase = 6.283185307f * (45*t + 55*.025f*(1-std::exp(-t/.025f)));
                float y = level * std::sin(phase) * std::exp(-18*t) * std::min(1.f,t/.002f);
                return std::array<float,2>{y,y};
            });
        };
        auto snare = [&](float when, float level) {
            add(when, .16f, [&](float t, std::size_t) {
                float y = level * (.72f*noise.next() + .28f*std::sin(6.283185307f*180*t)) * std::exp(-35*t);
                return std::array<float,2>{y,y*.9f};
            });
        };
        for (int bar = 0; bar < 8; ++bar) {
            float start = bar*4*beat;
            kick(start, 1); kick(start+2*beat,.85f); kick(start+2.75f*beat,.45f);
            snare(start+beat,.7f); snare(start+3*beat,.7f);
            snare(start+1.75f*beat,.13f); snare(start+3.75f*beat,.12f);
            for (int step = 0; step < 8; ++step) {
                float lp = 0, level = step%2 ? .08f : .16f;
                add(start+step*.5f*beat,.06f,[&](float t, std::size_t) {
                    float x = noise.next(); lp += .3f*(x-lp);
                    float y = level*(x-lp)*std::exp(-75*t);
                    return std::array<float,2>{y*.8f,y};
                });
            }
        }
    } else {
        for (int chord = 0; chord < 4; ++chord) {
            float duration = 8*beat;
            add(chord*duration, duration, [&](float t, std::size_t) {
                float envelope = std::min(1.f,t/.45f) * std::min(1.f,(duration-t)/.7f);
                float l = 0, r = 0;
                for (int note = 0; note < 4; ++note) {
                    float hz = noteHz(notes[chord][note]);
                    float phase = 6.283185307f * hz * t;
                    l += std::sin(phase) + .16f*std::sin(2*phase) + .06f*std::sin(3*phase);
                    r += std::sin(phase*1.0015f + note*.13f) + .16f*std::sin(2*phase*1.0015f);
                }
                return std::array<float,2>{l*.1f*envelope,r*.1f*envelope};
            });
        }
    }
    float peak = 0;
    for (std::size_t i = 0; i < frames; ++i) peak = std::max({peak,std::abs(clip.left[i]),std::abs(clip.right[i])});
    float gain = peak > 0 ? .32f / peak : 1;
    for (std::size_t i = 0; i < frames; ++i) { clip.left[i] *= gain; clip.right[i] *= gain; }
    return clip;
}
inline Galaxy auditionGalaxy(Galaxy g, Treatment treatment) {
    if (treatment != Treatment::Galaxy) for (std::size_t i = 0; i < g.count; ++i)
        g.rooms[i] = {.040f, treatment == Treatment::Half ? .5f : 1.f, .35f, treatment == Treatment::Reverse};
    return g;
}
} // namespace manyspace
