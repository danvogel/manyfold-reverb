#include "session.hpp"
#include "reference64.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>

static bool watch = false;
static std::size_t allocated = 0;
void* operator new(std::size_t n) {
    if (watch) ++allocated;
    if (auto* p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

std::vector<float> render(std::size_t block) {
    manyspace::Session s;
    s.prepare(manyspace::referenceGalaxy(),manyspace::generateClip(manyspace::Example::Plucks,48000),manyspace::scenarios()[2]);
    assert(s.queue.push({manyspace::CommandKind::Play,1}));
    auto n = std::size_t(48000*12);
    std::vector<float> l(n),r(n);
    for (std::size_t i = 0; i < n;) {
        auto size = std::min(block,n-i); s.process(l.data()+i,r.data()+i,size); i += size;
    }
    assert(s.telemetry.instances == 3); // Initial visit, at 5 and 10 seconds.
    return l;
}
int main() {
    using namespace manyspace;
    CommandQueue<4> queue;
    assert(queue.push({CommandKind::Room,1}) && queue.push({CommandKind::Room,2}) && queue.push({CommandKind::Room,3}));
    assert(!queue.push({CommandKind::Room,4}));
    Command command;
    for (int i = 1; i <= 3; ++i) { assert(queue.pop(command)); assert(command.a == i); }
    assert(!queue.pop(command));
    CommandQueue<> concurrent;
    std::thread producer([&] { for (int i = 0; i < 100000; ++i) while (!concurrent.push({CommandKind::Room,static_cast<float>(i)})) std::this_thread::yield(); });
    for (int i = 0; i < 100000; ++i) {
        while (!concurrent.pop(command)) std::this_thread::yield();
        assert(command.a == i);
    }
    producer.join();
    // Notebook Immersion blends dry/wet with fixed wet makeup gain; Studio's additive Effect is unchanged.
    auto blendRender=[](float mix, AuditionBus bus, bool change=false) {
        Session session;auto settings=scenarios()[0];settings.mix=mix;settings.automatic=false;settings.trigger=Trigger::Manual;
        session.prepare(referenceGalaxy(),generateClip(Example::Plucks,48000),settings);
        session.queue.push({CommandKind::Bus,static_cast<float>(bus)});
        session.queue.push({CommandKind::Play,1});
        std::vector<float> left(24000),right(24000);
        if(change) {
            session.process(left.data(),right.data(),6000);
            session.queue.push({CommandKind::Knobs,1,settings.decay,settings.sensitivity});
            session.process(left.data()+6000,right.data()+6000,18000);
        } else session.process(left.data(),right.data(),left.size());
        return left;
    };
    auto dryBlend=blendRender(0,AuditionBus::Immersion),dryBus=blendRender(0,AuditionBus::Dry);
    auto fullBlend=blendRender(1,AuditionBus::Immersion),wetBus=blendRender(1,AuditionBus::Wet);
    auto halfBlend=blendRender(.5f,AuditionBus::Immersion),additive=blendRender(1,AuditionBus::Effect);
    auto liveBlend=blendRender(0,AuditionBus::Immersion,true);
    float wetEnergy=0;
    for(std::size_t i=2000;i<24000;++i) {
        assert(std::abs(dryBlend[i]-dryBus[i])<1e-6f);
        assert(std::abs(fullBlend[i]-(4.f*wetBus[i]))<1e-6f);
        assert(std::abs(halfBlend[i]-(.5f*dryBus[i]+2.f*wetBus[i]))<1e-6f);
        assert(std::abs(additive[i]-dryBus[i]-wetBus[i])<1e-6f);
        if(i>16000)assert(std::abs(liveBlend[i]-fullBlend[i])<1e-6f);
        wetEnergy+=wetBus[i]*wetBus[i];
    }
    assert(wetEnergy>1e-5f);
    // Delay just above an integer: write-delay is slightly negative at the
    // boundary. Float-only wrapping used to round to ring length (ASan OOB).
    auto boundary = auditionGalaxy(referenceGalaxy(),Treatment::Normal);
    for (auto& room : boundary.rooms) room.distance = std::nextafter(.0625f,1.f);
    Core boundaryCore; boundaryCore.init(boundary,48000,4);
    std::vector<float> boundaryInput(7000,.1f), boundaryL(7000), boundaryR(7000);
    boundaryCore.process(boundaryInput.data(),boundaryInput.data(),boundaryL.data(),boundaryR.data(),7000);
    for (float x : boundaryL) assert(std::isfinite(x));
    assert(render(127) == render(511));
    // Built-in phrases loop on the musical boundary, not through export padding.
    for (float sr : {44100.f,48000.f,96000.f}) for (auto example : {Example::Plucks,Example::Drums,Example::Chords}) {
        auto clip = generateClip(example,sr);
        auto boundary = static_cast<std::size_t>(std::llround(32*60.0/clip.bpm*sr));
        Session looping; looping.prepare(referenceGalaxy(),clip,scenarios()[2]);
        std::vector<float> l(boundary+1),r(boundary+1);
        looping.queue.push({CommandKind::Play,1}); looping.process(l.data(),r.data(),l.size());
        assert(looping.telemetry.cursor == 1 && looping.telemetry.playing);
        assert(looping.telemetry.instances == 5 && looping.telemetry.room == 4);
        // One-shot playback still includes the six-second tail interval.
        looping.prepare(referenceGalaxy(),clip,scenarios()[0]);
        looping.queue.push({CommandKind::Loop,0});looping.queue.push({CommandKind::Play,1});
        looping.process(l.data(),r.data(),l.size());assert(looping.telemetry.cursor == boundary+1);
    }
    // Sensitivity in isolation: identical dry drums, Decay and traversal mode.
    auto drums = generateClip(Example::Drums,48000);
    std::uint64_t visits[2]{};
    for (int i = 0; i < 2; ++i) {
        auto scenario = scenarios()[3]; scenario.sensitivity = i ? .9f:.35f;
        Session audition; audition.prepare(referenceGalaxy(),drums,scenario);
        audition.queue.push({CommandKind::Play,1}); audition.queue.push({CommandKind::Loop,0});
        float left[512], right[512];
        for (std::size_t frame = 0; frame < drums.left.size(); frame += 512) audition.process(left,right,512);
        visits[i] = audition.telemetry.instances;
    }
    assert(visits[1]-1 > (visits[0]-1)*1.5);
    std::cout << "sensitivity-only drum jumps: " << visits[0]-1 << " -> " << visits[1]-1 << '\n';
    for (auto budget : {4u,8u}) {
        Session s; s.prepare(referenceGalaxy(),generateClip(Example::Drums,48000),scenarios()[4],budget);
        float l[1537]{},r[1537]{};
        s.process(l,r,1537); assert(s.telemetry.cursor == 0);
        assert(s.queue.push({CommandKind::Play,1})); s.process(l,r,1537);
        assert(s.telemetry.cursor == 1537);
        assert(s.queue.push({CommandKind::Play,0})); s.process(l,r,1537);
        auto paused = s.telemetry.cursor.load(); assert(paused == 1537+240);
        const auto voice=s.coreForOfflineTests().diagnostic(0);
        const auto instances=s.telemetry.instances.load();
        s.process(l,r,1537); assert(s.telemetry.cursor == paused && s.telemetry.instances == instances);
        const auto frozen=s.coreForOfflineTests().diagnostic(0);
        assert(frozen.gain==voice.gain && frozen.level==voice.level && frozen.age==voice.age);
        for(float sample:l)assert(sample==0);
        assert(s.queue.push({CommandKind::Play,1}));
        allocated = 0; watch = true;
        for (int k = 0; k < 250; ++k) {
            if (k%7 == 0) assert(s.queue.push({CommandKind::Leap}));
            if (k%13 == 0) assert(s.queue.push({CommandKind::Knobs,(k%3)*.5f,k%2 ? 8.f : .3f,k%2 ? 1.f : 0.f}));
            if (k%11 == 0) assert(s.queue.push({CommandKind::Bus,static_cast<float>(k%4)}));
            s.process(l,r,1537);
            assert(s.telemetry.grainGain >= 1 && s.telemetry.grainGain <= 3.00001f);
            for (float x : l) assert(std::isfinite(x) && std::abs(x) <= 1);
        }
        watch = false; assert(allocated == 0);
    }
    // Loop and nonloop end handling, larger-than-scratch callbacks, malformed inputs.
    Clip small; small.sampleRate = 48000; small.left.assign(100,.1f); small.right.assign(100,.1f);
    Session s; s.prepare(referenceGalaxy(),small,scenarios()[0]);
    float l[1000],r[1000];
    s.queue.push({CommandKind::Play,1}); s.process(l,r,250); assert(s.telemetry.cursor == 50);
    s.queue.push({CommandKind::Loop,0}); s.process(l,r,1000); assert(!s.telemetry.playing && s.telemetry.cursor == 100);
    // Native EOF Play prepares again off callback, restoring the user's controls.
    auto settings = scenarios()[0]; settings.mix = 0; settings.decay = 3.2f; settings.sensitivity = .9f;
    s.prepare(referenceGalaxy(),small,settings);
    assert(s.telemetry.cursor == 0 && !s.telemetry.playing);
    s.queue.push({CommandKind::Loop,0}); s.queue.push({CommandKind::Play,1});
    s.process(l,r,50); assert(s.telemetry.cursor == 50 && s.telemetry.playing);
    s.process(l,r,100); assert(s.telemetry.cursor == 100 && !s.telemetry.playing);
    // Custom sources skip only appended padding, including when Loop is enabled
    // after one-shot playback has already entered that padding.
    small.loopFrames=60;
    s.prepare(referenceGalaxy(),small,scenarios()[0]);
    s.queue.push({CommandKind::Loop,0});s.queue.push({CommandKind::Play,1});s.process(l,r,80);
    assert(s.telemetry.cursor==80);
    s.queue.push({CommandKind::Loop,1});s.process(l,r,5);assert(s.telemetry.cursor==5);
    auto invalid=small;invalid.loopFrames=101;
    bool rejected=false;try{s.prepare(referenceGalaxy(),invalid,scenarios()[0]);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    small.left[0] = std::numeric_limits<float>::quiet_NaN(); small.right[1] = 2;
    s.prepare(referenceGalaxy(),small,scenarios()[0]);
    assert(s.clipStopped().left[0] == 0 && s.clipStopped().right[1] == 1);
    std::cout << "session: SPSC, callbacks, block invariance, transport, looping, bounds, live controls, allocation OK\n";
}
