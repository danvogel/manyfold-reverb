#include "core.hpp"
#include "reference64.hpp"
#include <cassert>
#include <iostream>
#include <limits>

using namespace manyspace;
struct Audio { std::vector<float> left,right; };
Audio render(float spread,float sr,float speed,bool reverse,std::size_t block=127,float mix=1) {
    auto g=referenceGalaxy();
    for(auto& r:g.rooms)r={.04f,speed,.35f,reverse,6000,20,spread};
    Core c;c.init(g,sr,4);c.setKnobs(mix,2,0);
    float zero[127]{},a[127],b[127];
    for(unsigned k=0;k<static_cast<unsigned>(sr/127)+1;++k)c.process(zero,zero,a,b,127);
    Audio in,out;auto n=static_cast<std::size_t>(sr*2);
    for(auto* x:{&in.left,&in.right,&out.left,&out.right})x->resize(n);
    for(std::size_t i=0;i<n;++i) {
        in.left[i]=.15f*std::sin(6.283185307*311*i/sr)+.05f*std::sin(6.283185307*733*i/sr);
        in.right[i]=.12f*std::sin(6.283185307*457*i/sr)+.04f*std::sin(6.283185307*997*i/sr);
    }
    for(std::size_t i=0;i<n;) {
        auto size=std::min(block,n-i);
        c.process(in.left.data()+i,in.right.data()+i,out.left.data()+i,out.right.data()+i,size);i+=size;
    }
    for(std::size_t i=0;i<n;++i) {
        if(!mix)assert(out.left[i]==in.left[i] && out.right[i]==in.right[i]);
        out.left[i]-=in.left[i];out.right[i]-=in.right[i];
        assert(std::isfinite(out.left[i]) && std::isfinite(out.right[i]));
        assert(std::abs(out.left[i])<=1 && std::abs(out.right[i])<=1);
    }
    return out;
}
double energy(const Audio& a,bool side) {
    double sum=0;for(std::size_t i=0;i<a.left.size();++i) {
        double x=.5*(a.left[i]+(side ? -a.right[i]:a.right[i]));sum+=x*x;
    }return sum;
}
Audio departing(float oldSpread,float newSpread) {
    auto g=referenceGalaxy();g.rooms[0]={.04f,1,.35f,false,6000,20,oldSpread};
    g.rooms[1]={.04f,1,.35f,false,6000,20,newSpread};
    Core c;c.init(g,48000,4);c.setKnobs(1,4,0);
    std::vector<float> in(24000),a(in.size()),b(in.size());
    for(std::size_t i=0;i<in.size();++i)in[i]=.15f*std::sin(i*.037f);
    c.process(in.data(),in.data(),a.data(),b.data(),in.size());c.jump(1);
    std::fill(in.begin(),in.end(),0);Audio out;out.left.resize(in.size());out.right.resize(in.size());
    c.process(in.data(),in.data(),out.left.data(),out.right.data(),in.size());return out;
}
int main() {
    double worstMonoDb=0,worstHalfError=0;
    for(float sr:{8000.f,44100.f,48000.f,96000.f})for(float speed:{.5f,1.f,1.5f})for(bool reverse:{false,true}) {
        auto mono=render(0,sr,speed,reverse),half=render(.5f,sr,speed,reverse),wide=render(1,sr,speed,reverse);
        assert(energy(wide,true)>1e-5 && energy(mono,false)>1e-5);
        for(std::size_t i=0;i<mono.left.size();++i)assert(std::abs(mono.left[i]-mono.right[i])<1e-7);
        // Width changes side energy, rather than boosting the center to hide it.
        auto monoDb=std::abs(10*std::log10(energy(mono,false)/energy(wide,false)));
        auto halfError=std::abs(std::sqrt(energy(half,true)/energy(wide,true))-.5);
        worstMonoDb=std::max(worstMonoDb,monoDb);worstHalfError=std::max(worstHalfError,halfError);
        assert(monoDb<.05 && halfError<.005);
        render(0,sr,speed,reverse,127,0);render(1,sr,speed,reverse,127,0);
    }
    auto small=render(.37f,48000,.5f,true,127),large=render(.37f,48000,.5f,true,1024);
    assert(small.left==large.left && small.right==large.right);
    auto narrowTail=departing(0,1),narrowControl=departing(0,0);
    assert(narrowTail.left==narrowControl.left && narrowTail.right==narrowControl.right);
    assert(energy(narrowTail,true)==0 && energy(narrowTail,false)>1e-5);
    auto wideTail=departing(1,0),wideControl=departing(1,1);
    assert(wideTail.left==wideControl.left && wideTail.right==wideControl.right && energy(wideTail,true)>1e-5);
    for(std::size_t budget:{4u,8u}) {
        auto g=referenceGalaxy();for(std::size_t i=0;i<g.count;++i)g.rooms[i].spread=(i%5)/4.f;
        Core c;c.init(g,48000,budget);c.setKnobs(1,8,1);float in[127],a[127],b[127];
        for(unsigned k=0;k<1000;++k) {
            for(unsigned i=0;i<127;++i)in[i]=k<300 ? .2f*std::sin((127*k+i)*.04f):0;
            if(k<500)c.jump(k%64);c.process(in,in,a,b,127);
            for(unsigned i=0;i<127;++i)assert(std::isfinite(a[i]) && std::isfinite(b[i]) && std::abs(a[i]-in[i])<=1 && std::abs(b[i]-in[i])<=1);
        }
        for(float bad:{-.01f,1.01f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
            g.rooms[0].spread=bad;bool rejected=false;try{c.init(g,48000,budget);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
        }
    }
    std::cout<<"stereo: 24 rate/speed/direction cases; worst mono level difference "<<worstMonoDb
             <<" dB; half-spread side RMS error "<<worstHalfError<<"; dry unity, retained tail width, block invariance, rapid visits OK\n";
}
