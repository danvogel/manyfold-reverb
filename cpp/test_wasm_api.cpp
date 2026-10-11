// Drives the C ABI shim natively and checks it against Session driven directly.
#include "wasm_api.cpp"
#include "reference64.hpp"
#include <cstdio>
#include <functional>

using namespace manyspace;
static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } } while (0)

static std::uint64_t hashBuffer(const float* a, std::size_t n, std::uint64_t h = 1469598103934665603ull) {
    for (std::size_t i = 0; i < n; ++i) { std::uint32_t bits; std::memcpy(&bits, a+i, 4); h ^= bits; h *= 1099511628211ull; }
    return h;
}
static void fillFromGalaxy(const Galaxy& g) {
    for (std::size_t i = 0; i < g.count; ++i) {
        float* r = ms_rooms() + i*18; const Room& room = g.rooms[i];
        r[0] = room.distance; r[1] = room.speed; r[2] = room.damping; r[3] = room.swell ? 1 : 0; r[4] = room.lowpassHz; r[5] = room.highpassHz;
        r[6] = room.spread;r[7]=room.diffusion;r[8]=room.predelay;r[9]=room.reflections;
        r[10]=room.modulationDepth;r[11]=room.modulationRate;r[12]=room.shimmer;r[13]=room.hold;
        r[14]=room.drive;r[15]=room.ducking;r[16]=room.pan;r[17]=room.bloom;
        double sum = 0; for (std::size_t j = 0; j < g.count; ++j) sum += g.probabilities[i][j];
        for (std::size_t j = 0; j < g.count; ++j) ms_probabilities()[i*g.count+j] = static_cast<float>(g.probabilities[i][j]/sum);
    }
}
int main() {
    Galaxy g = referenceGalaxy();
    for (std::size_t i=0;i<g.count;++i){auto& r=g.rooms[i];r.spread=static_cast<float>(i%5)/4;r.diffusion=.5f;r.predelay=.03f;r.reflections=1+i%7;
        r.modulationDepth=.001f;r.modulationRate=.4f;r.shimmer=.1f;r.hold=.1f;r.drive=1.1f;r.ducking=.2f;r.pan=(static_cast<float>(i%5)-2)/4;r.bloom=.08f;}
    CHECK(ms_create(48000), "create");
    CHECK(ms_room_floats()==18,"character room ABI");
    CHECK(ms_max_rooms()==256,"room capacity ABI");
    CHECK(ms_prepare(99, .5f, 2, .5f, 0, 0, 8, 11, 0, 0, 0, 1, 0) == 0, "reject bad count");
    fillFromGalaxy(g);
    CHECK(ms_prepare(64, .55f, 2.8f, .65f, 0, 1, 8, 11, 0, 0, 0, 1, 5), "prepare 64");
    CHECK(ms_command(6, 1, 0, 0), "play");
    // Render 2 seconds in mixed block sizes through the shim.
    std::vector<float> shimL, shimR; std::size_t frames = 0;
    const int blocks[] = {128, 4096, 511, 1024, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096};
    for (int b : blocks) { CHECK(ms_process(b), "process"); shimL.insert(shimL.end(), ms_left(), ms_left()+b); shimR.insert(shimR.end(), ms_right(), ms_right()+b); frames += b; }
    bool finite = true; float peak = 0;
    for (std::size_t i = 0; i < frames; ++i) { finite = finite && std::isfinite(shimL[i]) && std::isfinite(shimR[i]); peak = std::max({peak, std::abs(shimL[i]), std::abs(shimR[i])}); }
    CHECK(finite, "finite output"); CHECK(peak > .01f && peak <= 1.f, "audible, bounded output");
    const float* t = ms_telemetry();
    CHECK(t[MS_T_PLAYING] == 1, "telemetry playing"); CHECK(t[MS_T_ROOM] == 5, "telemetry starting room");
    CHECK(static_cast<std::size_t>(t[MS_T_CURSOR]) == frames, "telemetry cursor"); CHECK(t[MS_T_RATE] == 48000, "telemetry rate");
    // Same thing through Session directly must match bit for bit.
    Session direct; Scenario sc = scenarios()[2]; sc.mix = .55f; sc.decay = 2.8f; sc.sensitivity = .65f; sc.automatic = false; sc.traversal = Traversal::Leap; sc.phraseNext = false;
    direct.prepare(g, generateClip(Example::Plucks, 48000), sc, 8, 11);
    direct.queue.push({CommandKind::Room, 5}); direct.queue.push({CommandKind::Loop, 1}); direct.queue.push({CommandKind::Play, 1});
    std::vector<float> dL(frames), dR(frames); std::size_t done = 0;
    for (int b : blocks) { direct.process(dL.data()+done, dR.data()+done, b); done += b; }
    CHECK(hashBuffer(shimL.data(), frames) == hashBuffer(dL.data(), frames) && hashBuffer(shimR.data(), frames) == hashBuffer(dR.data(), frames), "shim matches Session bit for bit");
    // Leap changes the destination; knobs and bus commands are accepted; bad commands are rejected.
    CHECK(ms_command(2, 0, 0, 0), "leap"); ms_process(1024);
    CHECK(ms_telemetry()[MS_T_ROOM] != 5, "leap moved destination");
    CHECK(ms_command(0, .2f, 1.f, .9f), "knobs"); CHECK(ms_command(8, 2, 0, 0), "wet bus"); CHECK(ms_command(8, 3, 0, 0), "immersion bus");
    CHECK(!ms_command(8, 4, 0, 0) && !ms_command(8, 1.5f, 0, 0), "invalid bus rejected");
    CHECK(ms_command(0, 2, 1, 1) == 0, "reject mix > 1"); CHECK(ms_command(9, 0, 0, 0) == 0, "reject unknown kind");
    CHECK(ms_process(5000) == 0, "reject oversize block");
    // Active voices appear in telemetry after a leap.
    for (int i = 0; i < 20; ++i) ms_process(4096);
    int active = 0; for (int v = 0; v < 8; ++v) active += ms_telemetry()[MS_T_VOICES + v*7 + 5] > 0;
    CHECK(active >= 2, "two visits produce two active voices");
    // 16-room reference and reverse/half treatments prepare too.
    Galaxy g16 = g; g16.count = 16; fillFromGalaxy(g16);
    ms_rooms()[6] = 1.01f; CHECK(ms_prepare(16, .5f, 1.f, .5f, 1, 0, 4, 3, 2, 2, 1, 0, 0)==0,"reject bad spread");
    fillFromGalaxy(g16);
    for(unsigned field=7;field<18;++field) {
        const float saved=ms_rooms()[field];ms_rooms()[field]=NAN;
        CHECK(ms_prepare(16,.5f,1,.5f,0,0,4,3,0,0,0,1,0)==0,"reject nonfinite character property");
        ms_rooms()[field]=saved;
    }
    ms_rooms()[9]=1.5f;CHECK(ms_prepare(16,.5f,1,.5f,0,0,4,3,0,0,0,1,0)==0,"reject fractional reflection pattern");fillFromGalaxy(g16);
    CHECK(ms_prepare(16, .5f, 1.f, .5f, 1, 0, 4, 3, 2, 2, 1, 0, 0), "prepare 16 reverse");
    CHECK(ms_prepare(16, .5f, 1.f, .5f, 1, 0, 4, 3, 1, 3, 0, 1, 15), "prepare 16 half");
    CHECK(ms_command(3,16,0,0)==0,"reject position outside the prepared galaxy");
    CHECK(ms_command(3,1.5f,0,0)==0,"reject fractional room selection");
    ms_probabilities()[0] = 2; CHECK(ms_prepare(16, .5f, 1.f, .5f, 0, 0, 4, 3, 0, 0, 0, 1, 0) == 0, "reject bad probability");
    CHECK(std::strlen(ms_error()) > 0, "error message present");
    // A custom 1000-frame stereo source crosses multiple boundaries in a 4096 callback.
    fillFromGalaxy(g16);
    CHECK(ms_source_ptr(0)==nullptr && ms_source_ptr(ms_source_max_frames()+1)==nullptr,"reject custom size");
    auto* source=ms_source_ptr(1000);
    CHECK(source!=nullptr,"allocate custom source off callback");
    for(int i=0;i<1000;i++){source[2*i]=.2f*std::sin(i*.1f);source[2*i+1]=-.1f*std::sin(i*.1f);}
    CHECK(ms_prepare(16,.5f,1,.5f,3,0,4,3,3,0,0,1,0)==0,"reject invalid trigger prepare");
    for(int mode : {0,2}) {
        CHECK(ms_prepare(16,.5f,1,.5f,mode,0,4,3,3,0,0,1,0),"custom prepare");
        ms_command(6,1,0,0);ms_process(4096);
        CHECK(ms_telemetry()[MS_T_PASSES]==4,"exact boundary count within one callback");
        CHECK(ms_telemetry()[MS_T_ROOM]==(mode==2 ? 4:0),"loop mode traverses once per boundary; manual never does");
        CHECK(ms_telemetry()[MS_T_CURSOR]==96,"custom loops at decoded boundary");
    }
    CHECK(ms_command(4,2.5f,0,0)==0 && ms_command(4,-1,0,0)==0,"trigger command bounds");
    CHECK(ms_prepare(16,.5f,1,.5f,2,0,4,3,3,0,0,0,0),"custom once");
    ms_command(6,1,0,0);ms_process(1000);
    CHECK(ms_telemetry()[MS_T_PASSES]==1 && ms_telemetry()[MS_T_ROOM]==1,"once fires at exact end");
    ms_process(4096);CHECK(ms_telemetry()[MS_T_PASSES]==1 && ms_telemetry()[MS_T_ROOM]==1,"no repeat at stopped end");
    ms_command(6,1,0,0);ms_process(1000);
    CHECK(ms_telemetry()[MS_T_PASSES]==2 && ms_telemetry()[MS_T_ROOM]==2,"once restarts in new room");
    ms_destroy(); CHECK(ms_command(1, 0, 0, 0) == 0, "no commands after destroy");
    std::printf(failures ? "wasm_api: %d failure(s)\n" : "wasm_api: all checks passed\n", failures);
    return failures ? 1 : 0;
}
