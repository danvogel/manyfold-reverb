#include "core.hpp"
#include "reference64.hpp"
#include <cassert>
#include <iostream>

double energy(float sr, float frequency, float speed, bool reverse, float highcut, float lowcut) {
    auto g = manyspace::referenceGalaxy();
    for (auto& room : g.rooms) room = {.040f,speed,.35f,reverse,highcut,lowcut};
    manyspace::Core core; core.init(g,sr,4); core.setKnobs(1,.3f,0);
    std::vector<float> input(static_cast<std::size_t>(2*sr)),left(input.size()),right(input.size());
    for (std::size_t i=0; i<input.size(); ++i) input[i]=.03f*std::sin(6.283185307*frequency*i/sr);
    core.process(input.data(),input.data(),left.data(),right.data(),input.size());
    double sum=0;
    for (std::size_t i=static_cast<std::size_t>(sr); i<input.size(); ++i) {
        double a=left[i]-input[i], b=right[i]-input[i];
        assert(std::isfinite(a) && std::isfinite(b) && std::abs(a)<1 && std::abs(b)<1);
        sum+=a*a+b*b;
    }
    return sum/sr;
}
int main() {
    double worstHigh=0, worstLow=0;
    for (float sr : {44100.f,48000.f,96000.f}) for (float speed : {.5f,1.f,1.5f}) for (bool reverse : {false,true}) {
        const double high=energy(sr,8000,speed,reverse,1200,20)/energy(sr,8000,speed,reverse,16000,20);
        const double low=energy(sr,30,speed,reverse,6000,500)/energy(sr,30,speed,reverse,6000,20);
        worstHigh=std::max(worstHigh,high); worstLow=std::max(worstLow,low);
        assert(high<.02 && low<.02);
        // Cut the harsh top / rumble while retaining midrange body.
        assert(energy(sr,500,speed,reverse,1200,20)/energy(sr,500,speed,reverse,16000,20)>.4);
        // Keep the output tone at 1.5 kHz across varispeed settings.
        assert(energy(sr,1500/speed,speed,reverse,6000,500)/energy(sr,1500/speed,speed,reverse,6000,20)>.85);
    }
    // Extreme supported filter settings/rates, changing rooms, independent tails.
    for (float sr : {8000.f,44100.f,96000.f}) {
        auto g=manyspace::referenceGalaxy();
        for (std::size_t i=0; i<g.count; ++i) {
            g.rooms[i].lowpassHz=i%2 ? 16000:800;
            g.rooms[i].highpassHz=i%2 ? 1000:10;
        }
        manyspace::Core core; core.init(g,sr,8); core.setKnobs(1,8,1);
        float input[127]{},left[127],right[127];
        for (int k=0; k<1000; ++k) {
            for (int i=0; i<127; ++i) input[i]=k<300 ? .25f*std::sin((127*k+i)*.07f):0;
            if (k%17==0)core.next();
            core.process(input,input,left,right,127);
            for (int i=0; i<127; ++i) assert(std::isfinite(left[i]) && std::isfinite(right[i]) && std::abs(left[i]-input[i])<=1 && std::abs(right[i]-input[i])<=1);
        }
        core.setKnobs(0,8,1);
        // Once the knob settles, even zero dry input must remain exactly zero.
        std::fill(std::begin(input),std::end(input),0);
        for (unsigned k=0; k<static_cast<unsigned>(sr/127)+1; ++k)core.process(input,input,left,right,127);
        for (int k=0; k<50; ++k) {
            if (k<25)for (int i=0; i<127; ++i)input[i]=.2f*std::sin((127*k+i)*.05f);
            else std::fill(std::begin(input),std::end(input),0);
            core.process(input,input,left,right,127);
            for (int i=0; i<127; ++i)assert(left[i]==input[i] && right[i]==input[i]);
        }
        auto bad=g;bad.rooms[0].highpassHz=bad.rooms[0].lowpassHz;
        bool rejected=false;try{core.init(bad,sr,4);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    }
    std::cout<<"room filters: 18 rate/speed/direction pairs; worst 8 kHz / 30 Hz energy ratios "<<worstHigh<<" / "<<worstLow<<"; midrange, dry unity, corner stability OK\n";
}
