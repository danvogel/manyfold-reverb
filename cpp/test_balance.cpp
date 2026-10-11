#include "core.hpp"
#include "reference64.hpp"
#include "listening.hpp"
#include <cassert>
#include <iostream>

double wetEnergy(float sr, int fixture, bool reverse) {
    auto g = manyspace::auditionGalaxy(manyspace::referenceGalaxy(),reverse ? manyspace::Treatment::Reverse:manyspace::Treatment::Normal);
    manyspace::Core core; core.init(g,sr,8); core.setKnobs(.7f,2.2f,.65f);
    std::vector<float> input(static_cast<std::size_t>(sr*2)), l(input.size()), r(input.size());
    manyspace::Noise noise;
    const float tones[] = {55,87.5f,110,220,440,880,1000,2000};
    for (std::size_t i = 0; i < input.size(); ++i) {
        float t = i/sr;
        input[i] = fixture < 8 ? .2f*std::sin(6.283185307f*tones[fixture]*t)
                 : fixture == 8 ? .18f*noise.next() : std::fmod(t,.13f) < .01f ? .3f*noise.next():0;
    }
    core.process(input.data(),input.data(),l.data(),r.data(),input.size());
    assert(core.maximumGrainGain() >= 1 && core.maximumGrainGain() <= 3.00001f);
    double energy = 0;
    for (std::size_t i = static_cast<std::size_t>(sr*.6f); i < input.size(); ++i) {
        float a = l[i]-input[i], b = r[i]-input[i];
        assert(std::isfinite(a) && std::abs(a) < 1 && std::isfinite(b) && std::abs(b) < 1);
        energy += a*a+b*b;
    }
    return energy;
}
int main() {
    double maximum = 0;
    for (float sr : {44100.f,48000.f,96000.f}) for (int fixture = 0; fixture < 10; ++fixture) {
        double forward = wetEnergy(sr,fixture,false), reverse = wetEnergy(sr,fixture,true);
        double difference = 10*std::log10(reverse/forward);
        maximum = std::max(maximum,std::abs(difference));
        if (std::abs(difference) > 2) std::cerr << "balance failure " << sr << " fixture " << fixture << " dB " << difference << '\n';
        assert(std::abs(difference) <= 2);
    }
    std::cout << "reverse balance: 30 tone/noise/transient pairs at 44.1/48/96 kHz; maximum difference " << maximum << " dB\n";
}
