#include "core.hpp"
#ifdef USE_GALAXY64
#include "reference64.hpp"
#else
#include "reference16.hpp"
#endif
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

static bool watchAlloc = false;
static std::size_t allocations = 0;
void* operator new(std::size_t n) {
    if (watchAlloc) ++allocations;
    if (auto* p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace manyspace;
Galaxy uniform(bool swell = false, float speed = 1) {
    auto g = referenceGalaxy();
    for (std::size_t i = 0; i < g.count; ++i) g.rooms[i] = {.030f, speed, .3f, swell};
    return g;
}
std::vector<float> render(std::size_t block, float sr, std::size_t budget) {
    Core core; core.init(uniform(), sr, budget); core.setKnobs(.6f, 1.5f, .8f);
    std::size_t n = static_cast<std::size_t>(sr * 3);
    std::vector<float> in(n), out(n), other(n);
    for (std::size_t i = 0; i < n; ++i)
        in[i] = .3f * std::sin(6.283185307f * 220.f * i / sr) * std::exp(-5.f * i / sr);
    auto event = static_cast<std::size_t>(sr * .5f);
    for (std::size_t i = 0; i < n;) {
        if (i % event == 0 && i) core.leap();
        auto size = std::min({block, n - i, event - (i % event)});
        core.process(in.data() + i, in.data() + i, out.data() + i, other.data() + i, size);
        i += size;
    }
    return out;
}
float rms(const std::vector<float>& a, std::size_t start, std::size_t end) {
    double sum = 0;
    for (auto i = start; i < end; ++i) sum += a[i] * a[i];
    return std::sqrt(sum / (end - start));
}

int main() {
    assert(std::abs(Core::ageTarget(1) - std::pow(10.f, -.3f)) < 1e-6f);
    assert(std::abs(Core::ageTarget(7) - .0001f) < 1e-8f);
    // An audible old contribution must behave identically on a silent revisit
    // and a silent visit to another room with the same descriptor.
    {
        Core a, b; a.init(uniform(), 48000, 4); b.init(uniform(), 48000, 4);
        float zero[127]{}, in[127]{}, al[127]{}, ar[127]{}, bl[127]{}, br[127]{};
        for (int i = 0; i < 10; ++i) { a.process(zero, zero, al, ar, 127); b.process(zero, zero, bl, br, 127); }
        in[0] = .5f;
        a.process(in, in, al, ar, 127); b.process(in, in, bl, br, 127);
        for (int i = 0; i < 20; ++i) { a.process(zero, zero, al, ar, 127); b.process(zero, zero, bl, br, 127); }
        assert(a.diagnostic(0).level > .001f);
        auto identity = a.diagnostic(0).instance;
        a.jump(1); b.jump(1);
        for (int i = 0; i < 10; ++i) { a.process(zero, zero, al, ar, 127); b.process(zero, zero, bl, br, 127); }
        a.jump(0); b.jump(2);
        float peak = 0;
        for (int i = 0; i < 100; ++i) {
            a.process(zero, zero, al, ar, 127); b.process(zero, zero, bl, br, 127);
            for (int j = 0; j < 127; ++j) { assert(al[j] == bl[j] && ar[j] == br[j]); peak = std::max(peak, std::abs(al[j])); }
        }
        assert(peak > .00001f && a.diagnostic(0).instance == identity && a.diagnostic(0).age == 2);
    }
    // Playback RNG is explicit and independent of preset generation provenance.
    {
        Core a, b, c;
        a.init(referenceGalaxy(), 48000, 4, 23); b.init(referenceGalaxy(), 48000, 4, 23);
        c.init(referenceGalaxy(), 48000, 4, 24);
        bool different = false;
        for (int i = 0; i < 32; ++i) {
            a.leap(); b.leap(); c.leap();
            assert(a.currentRoom() == b.currentRoom());
            different |= a.currentRoom() != c.currentRoom();
        }
        assert(different);
    }
    for (auto budget : {4u, 8u}) {
        {
            Core e; e.init(uniform(), 48000, budget); e.setKnobs(.7f, 8, .5f);
            float tone[127]{}, el[127]{}, er[127]{};
            for (std::size_t v = 0; v < budget; ++v) {
                if (v) e.jump(v);
                for (int k = 0; k < 60; ++k) {
                    for (int j = 0; j < 127; ++j) tone[j] = (.05f + .03f * v) * std::sin((k * 127 + j) * .05f);
                    e.process(tone, tone, el, er, 127);
                }
            }
            std::size_t quietest = 0;
            for (std::size_t i = 0; i < budget; ++i) {
                assert(e.diagnostic(i).level > .000001f);
                if (e.diagnostic(i).level * e.diagnostic(i).gain < e.diagnostic(quietest).level * e.diagnostic(quietest).gain) quietest = i;
            }
            e.jump(15);
            for (std::size_t i = 0; i < budget; ++i) assert(e.diagnostic(i).pending == (i == quietest));
        }
        // Extreme-range inputs, nondefault timing, rapid pending replacement.
        for (float speed : {.5f, 1.5f}) {
            auto g = uniform(true, speed);
            for (std::size_t i = 0; i < g.count; ++i) g.rooms[i].distance = .015f, g.rooms[i].damping = .2f;
            Config config; config.departureSeconds = .003f; config.arrivalSeconds = .002f;
            config.arrivalFadeSeconds = .002f; config.tailRampSeconds = .007f;
            Core e; e.init(g, 96000, budget, 17, config); e.setKnobs(1, 8, 1);
            float in[31]{}, out[31]{}, right[31]{};
            float previousWet = 0, maxStep = 0;
            for (int k = 0; k < 15000; ++k) {
                for (int j = 0; j < 31; ++j) in[j] = std::sin((k * 31 + j) * .014f);
                if (k % 13 == 0) e.leap();
                e.process(in, in, out, right, 31);
                for (int j = 0; j < 31; ++j) {
                    assert(std::isfinite(out[j]) && std::abs(out[j]) < 2.f);
                    float wet = out[j] - in[j]; maxStep = std::max(maxStep, std::abs(wet - previousWet)); previousWet = wet;
                }
            }
            assert(maxStep < .10f);
        }
        for (float sr : {44100.f, 48000.f, 96000.f}) {
            auto a = render(1, sr, budget), b = render(127, sr, budget), c = render(511, sr, budget);
            assert(a == b && b == c);
            for (float x : a) assert(std::isfinite(x) && std::abs(x) < 1.3f);
        }
        Core core; core.init(uniform(), 48000, budget);
        assert(core.memoryBytes() < budget * 160000u);
        float in[127]{}, out[127]{}, right[127]{};
        in[0] = .5f;
        core.process(in, in, out, right, 127);
        auto old = core.diagnostic(0);
        core.jump(1); core.jump(0);
        assert(core.diagnostic(0).instance == old.instance && core.diagnostic(0).age == 2);
        assert(core.diagnostic(2).room == 0 && core.diagnostic(2).instance != old.instance);
        float target = core.diagnostic(0).target;
        assert(target < Core::ageTarget(1) && core.diagnostic(2).target == 1);
        float zero[127]{};
        for (int i = 0; i < 20; ++i) core.process(zero, zero, out, right, 127);
        assert(core.diagnostic(0).gain <= target + 1e-7f);
        // Force eviction and supersede a pending destination during its fade.
        for (std::size_t i = 0; i < budget; ++i) core.jump((i + 1) % 16);
        auto instances = core.createdInstances();
        core.jump(15);
        for (int i = 0; i < 20; ++i) core.process(zero, zero, out, right, 127);
        assert(core.createdInstances() == instances + 1);
        bool arrived = false;
        for (std::size_t i = 0; i < budget; ++i) if (core.diagnostic(i).age == 0 && core.diagnostic(i).room == 15) arrived = true;
        assert(arrived);
        allocations = 0; watchAlloc = true;
        float peakDelta = 0, previous = 0;
        for (int k = 0; k < 2000; ++k) {
            for (int j = 0; j < 127; ++j) in[j] = .05f * std::sin((k * 127 + j) * .01f);
            if (k % 3 == 0) core.leap();
            core.process(in, in, out, right, 127);
            for (float x : out) { assert(std::isfinite(x) && std::abs(x) < 1.05f); peakDelta = std::max(peakDelta, std::abs(x - previous)); previous = x; }
        }
        watchAlloc = false;
        assert(allocations == 0 && peakDelta < .03f);
    }
    // Unity dry remains exact at Mix=0, after the intentional knob smoothing.
    Core dry; dry.init(uniform(), 48000, 4); dry.setKnobs(0, 2, .5f);
    float zero[48000]{}, out[48000]{}, right[48000]{};
    dry.process(zero, zero, out, right, 48000);
    float input[127]; for (int i = 0; i < 127; ++i) input[i] = .4f * std::sin(i * .1f);
    dry.process(input, input, out, right, 127);
    for (int i = 0; i < 127; ++i) assert(out[i] == input[i] && right[i] == input[i]);
    // Actual delayed sound and late decay at multiple sample rates, without jumps.
    for (float sr : {44100.f, 48000.f, 96000.f}) {
        Core c; c.init(uniform(), sr, 4); c.setKnobs(1, 1, .5f);
        std::size_t n = static_cast<std::size_t>(sr * 4);
        std::vector<float> in(n), l(n), r(n);
        // Equal-duration pulse across rates: a single sample's physical bandwidth
        // changes with sample rate and is intentionally reduced by room filters.
        const auto onset=static_cast<std::size_t>(sr*.1f), duration=static_cast<std::size_t>(sr*.001f);
        for (std::size_t i=0; i<duration; ++i)in[onset+i]=.6f*std::sin(3.141592653589793*i/duration);
        c.process(in.data(), in.data(), l.data(), r.data(), n);
        for (std::size_t i = 0; i < n; ++i) l[i] -= in[i];
        assert(rms(l, sr * .12f, sr * .4f) > .0003f);
        assert(rms(l, sr * 2.f, sr * 3.f) < rms(l, sr * .3f, sr * .6f) * .01f);
    }
    {
        float reference = 0;
        for (float sr : {48000.f, 44100.f, 96000.f}) {
            Core c; c.init(uniform(), sr, 4); c.setKnobs(1, 1, .5f);
            std::size_t n = static_cast<std::size_t>(sr);
            std::vector<float> in(n), l(n), r(n);
            for (std::size_t i = 0; i < n; ++i) {
                float t = i / sr;
                if (t >= .1f && t < .15f) in[i] = .2f * std::sin(6.283185307f * 220 * t);
            }
            c.process(in.data(), in.data(), l.data(), r.data(), n);
            float energy = rms(l, sr * .2f, sr * .5f);
            if (!reference) reference = energy;
            assert(std::abs(energy / reference - 1) < .05f);
        }
    }
    // Only dry input drives automatic traversal; held notes and wet tails cannot chatter.
    Core trigger; trigger.init(uniform(), 48000, 4); trigger.setKnobs(1, 8, 1); trigger.setAutomatic(true);
    std::vector<float> held(48000, .1f), a(48000), b(48000);
    trigger.process(held.data(), held.data(), a.data(), b.data(), held.size());
    auto first = trigger.createdInstances(); assert(first == 2);
    trigger.process(zero, zero, a.data(), b.data(), 48000);
    assert(trigger.createdInstances() == first);
    trigger.process(held.data(), held.data(), a.data(), b.data(), held.size());
    assert(trigger.createdInstances() == first + 1);
    trigger.init(uniform(), 48000, 4); trigger.setKnobs(.5f, 2, 1);
    trigger.setTraversal(Traversal::Next); trigger.setAutomatic(true);
    trigger.process(held.data(), held.data(), a.data(), b.data(), 1000);
    assert(trigger.currentRoom() == 1 && trigger.createdInstances() == 2);
    trigger.process(zero, zero, a.data(), b.data(), 100);  // too short to rearm/cool down
    trigger.process(held.data(), held.data(), a.data(), b.data(), 1000);
    assert(trigger.currentRoom() == 1 && trigger.createdInstances() == 2);
    // Invalid preset/rate rejected before any callback.
    bool rejected = false;
    try { Core c; c.init(uniform(), 0, 4); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    // Finite guards for malformed host inputs.
    input[0] = std::numeric_limits<float>::infinity(); input[1] = std::numeric_limits<float>::quiet_NaN();
    dry.process(input, input, out, right, 127); assert(std::isfinite(out[0]) && std::isfinite(out[1]));
    std::cout << "core: dry, tails/revisit, 4/8 eviction, allocation, blocks, rates, decay, triggers OK\n";
}
