#include "core.hpp"
#include "reference64.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
using namespace manyspace;
static bool watch=false;static unsigned allocations=0;
void* operator new(std::size_t n){if(watch)++allocations;if(void* p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
struct Audio{std::vector<float> l,r;};
Galaxy galaxy(Room room){auto g=referenceGalaxy();for(auto& r:g.rooms)r=room;return g;}
Audio render(Room room,float sr=48000,unsigned block=127,bool immediate=false,float decay=4,float mix=1) {
    Core c;c.init(galaxy(room),sr,4);c.setKnobs(mix,decay,0);
    std::vector<float> zero(static_cast<unsigned>(sr)),a(zero.size()),b(zero.size());
    if(!immediate)c.process(zero.data(),zero.data(),a.data(),b.data(),zero.size());
    const auto n=static_cast<unsigned>(sr*3);std::vector<float> in(n);Audio out;out.l.resize(n);out.r.resize(n);
    // A short broadband excitation reveals reflection density and late energy.
    for(unsigned i=0;i<static_cast<unsigned>(sr*.025);++i)in[i]=.2f*std::sin(i*.731f)*std::exp(-static_cast<float>(i)/(sr*.005f));
    for(unsigned i=0;i<n;){auto size=std::min(block,n-i);watch=true;c.process(in.data()+i,in.data()+i,out.l.data()+i,out.r.data()+i,size);watch=false;i+=size;}
    for(unsigned i=0;i<n;++i){out.l[i]-=in[i];out.r[i]-=in[i];if(!std::isfinite(out.l[i]) || !std::isfinite(out.r[i])){std::cerr<<"nonfinite rate "<<sr<<" sample "<<i<<" block "<<block<<"\n";std::abort();}assert(std::abs(out.l[i])<=1 && std::abs(out.r[i])<=1);}
    return out;
}
double energy(const Audio& a,unsigned first=0,unsigned last=0){if(!last)last=a.l.size();double e=0;for(unsigned i=first;i<last;++i)e+=double(a.l[i])*a.l[i]+double(a.r[i])*a.r[i];return e;}
unsigned onset(const Audio& a){for(unsigned i=0;i<a.l.size();++i)if(std::abs(a.l[i])>1e-8)return i;return a.l.size();}
Room base(){return {.04f,1,.35f,false,6000,20,1};}
Room full(){auto r=base();r.diffusion=.8f;r.predelay=.12f;r.reflections=7;r.modulationDepth=.006f;r.modulationRate=1.5f;
    r.shimmer=.4f;r.hold=1;r.drive=6;r.ducking=.8f;r.pan=.6f;r.bloom=.6f;return r;}
int main(){
    // Same samples regardless of host block size, at each supported production rate.
    for(float sr:{8000.f,44100.f,48000.f,96000.f}) {
        auto r=full();r.speed=.5f;r.swell=true;
        auto a=render(r,sr,1),b=render(r,sr,127),c=render(r,sr,511);
        assert(a.l==b.l && a.r==b.r && a.l==c.l && a.r==c.r);assert(energy(a)>1e-6);
        auto plain=render(base(),sr);r=base();r.predelay=.07f;auto delayed=render(r,sr);
        assert(std::abs(double(onset(delayed))-onset(plain)-std::round(.07*sr))<=2);
        r=base();r.diffusion=.75f;auto diffused=render(r,sr);
        double ratio=10*std::log10(energy(diffused,static_cast<unsigned>(sr*.1))/energy(plain,static_cast<unsigned>(sr*.1)));
        std::cout<<"diffusion late energy "<<sr<<" Hz: "<<ratio<<" dB\n";assert(std::abs(ratio)<6);
    }
    auto plain=render(base());
    // Every independent addition must change a measurable output, not just metadata.
    for(unsigned feature=0;feature<10;++feature){auto r=base();
        switch(feature){case 0:r.diffusion=.7f;break;case 1:r.predelay=.07f;break;case 2:r.reflections=3;break;
        case 3:r.modulationDepth=.003f;break;case 4:r.speed=1.5f;r.shimmer=.4f;break;case 5:r.hold=1;break;
        case 6:r.drive=4;break;case 7:r.ducking=.8f;break;case 8:r.pan=1;break;case 9:r.bloom=.6f;break;}
        auto changed=render(r,48000,127,feature==9);assert(changed.l!=plain.l || changed.r!=plain.r);assert(energy(changed)>1e-7);
        if(feature==2)assert(onset(changed)<onset(plain));
        if(feature==8){assert(energy(changed)>0);for(float x:changed.l)assert(x==0);}
    }
    auto bloom=base();bloom.bloom=.6f;
    auto captured=render(bloom,48000,127,true);assert(energy(captured)>1e-6);
    // A held tail remains distinct from a fresh visit and continues aging monotonically.
    for(unsigned budget:{4u,8u})for(float speed:{.5f,1.5f})for(bool reverse:{false,true}) {
        auto r=full();r.speed=speed;r.swell=reverse;auto g=galaxy(r);Core c;c.init(g,48000,budget);c.setKnobs(1,8,0);
        float input[127],a[127],b[127];double maxMs=0,late=0;
        for(unsigned k=0;k<4000;++k){for(unsigned i=0;i<127;++i)input[i]=k<1000 ? .25f*std::sin((k*127+i)*.037f):0;
            auto start=std::chrono::steady_clock::now();watch=true;
            if(k>0 && k<1200 && k%29==0)c.jump(k%64);
            c.process(input,input,a,b,127);watch=false;
            maxMs=std::max(maxMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            for(unsigned i=0;i<127;++i){assert(std::isfinite(a[i]) && std::isfinite(b[i]));assert(std::abs(a[i]-input[i])<=1 && std::abs(b[i]-input[i])<=1);if(k>3900)late+=double(a[i])*a[i]+double(b[i])*b[i];}
            for(unsigned v=0;v<budget;++v){auto d=c.diagnostic(v);if(d.active && d.age>=7 && !d.pending)assert(d.target<=.0001001f);}
        }
        std::cout<<"stress budget "<<budget<<" speed "<<speed<<" reverse "<<reverse<<": max127 "<<maxMs<<" ms, late "<<late<<", memory "<<c.memoryBytes()<<" bytes\n";
    }
    // Drive does not add small-signal gain; ducking recovers after the dry stops.
    auto duck=base();duck.ducking=.8f;auto suppressed=render(duck);
    assert(energy(suppressed,0,4800)<energy(plain,0,4800));
    assert(std::abs(energy(suppressed,48000)/energy(plain,48000)-1)<.02);
    // Shimmer alone must decay after excitation, even at maximum regeneration/Decay.
    for(float speed:{.5f,1.5f}){auto r=base();r.speed=speed;r.shimmer=.4f;auto a=render(r,48000,127,false,8);
        assert(energy(a,96000)<energy(a,4800,52800));}
    auto dry=render(full(),48000,127,false,4,0);assert(energy(dry)==0);
    auto held=base();held.diffusion=.65f;auto unheld=render(held);held.hold=1;
    assert(energy(render(held),48000)>energy(unheld,48000)*2);
    // A visit's tail keeps its own character, regardless of its silent destination.
    {auto g=galaxy(full()),other=g;other.rooms[1]=base();Core a,b;a.init(g,48000,4);b.init(other,48000,4);a.setKnobs(1,8,0);b.setKnobs(1,8,0);
        float in[127],al[127],ar[127],bl[127],br[127];
        for(unsigned k=0;k<600;++k){for(unsigned i=0;i<127;++i)in[i]=k<300 ? .15f*std::sin((k*127+i)*.02f):0;
            if(k==300){a.jump(1);b.jump(1);}a.process(in,in,al,ar,127);b.process(in,in,bl,br,127);
            for(unsigned i=0;i<127;++i)assert(al[i]==bl[i] && ar[i]==br[i]);
        }
    }
    // 16-room slice and 4096-frame browser-sized workload with eight live visits.
    {auto g=galaxy(full());g.count=16;for(unsigned i=0;i<16;++i)for(unsigned j=0;j<16;++j)g.probabilities[i][j]=1.f/16;
        Core c;c.init(g,48000,8);c.setKnobs(1,8,0);float in[4096],a[4096],b[4096];for(unsigned i=0;i<4096;++i)in[i]=.1f*std::sin(i*.03f);
        for(unsigned j=0;j<8;++j){c.jump(j);c.process(in,in,a,b,4096);}double worst=0;
        for(unsigned j=0;j<50;++j){auto start=std::chrono::steady_clock::now();watch=true;c.process(in,in,a,b,4096);watch=false;
            worst=std::max(worst,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
        std::cout<<"8-voice native 4096-frame max "<<worst<<" ms (85.33 ms deadline)\n";
    }
    // Invalid descriptor corners, including fractional patterns, are rejected.
    float Room::* fields[]={&Room::diffusion,&Room::predelay,&Room::reflections,&Room::modulationDepth,&Room::modulationRate,&Room::shimmer,&Room::hold,&Room::drive,&Room::ducking,&Room::pan,&Room::bloom};
    for(auto field:fields){auto r=base();r.*field=NAN;assert(!r.validCharacter());bool rejected=false;try{Core c;c.init(galaxy(r),48000,4);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
    auto bad=base();bad.reflections=1.5f;assert(!bad.validCharacter());
    assert(allocations==0);std::cout<<"character: independent effects, onset timing, attack capture, decay, 4-rate exact block invariance, 4/8-tail stress, zero callback allocations OK\n";
}
