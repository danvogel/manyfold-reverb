"use strict";
// Drives the compiled WASM core through web/wasm-host.js with a fake AudioContext,
// exactly as the hosted page would, without a browser. Requires `make wasm`.
const assert=require("node:assert/strict"),fs=require("node:fs");
const Host=require("../web/wasm-host.js"),M=require("../web/model.js");
const text=fs.readFileSync("build/wasm/manyspace-core.js","utf8");
assert.ok(text.length<2*1024*1024,"core module stays below Qollab's 2 MB file cap");
const scene64=M.load(JSON.parse(fs.readFileSync("build/workflow/scene.json","utf8"))).scene;
const scene16=M.load(JSON.parse(fs.readFileSync("build/workflow/scene16.json","utf8"))).scene;
let processors=[];
class FakeContext {
  constructor(){this.sampleRate=48000;this.state="suspended";this.currentTime=0;this.destination={};}
  createScriptProcessor(size,inputs,outputs){assert.equal(size,4096);const node={size,outputs,connect(){},disconnect(){},onaudioprocess:null};processors.push(node);return node;}
  async resume(){this.state="running";}
  async close(){this.state="closed";}
}
globalThis.AudioContext=FakeContext;
// Node has no `require` inside Function scope; Emscripten's node branch needs it. The browser never does.
const browserEvaluate=code=>new Function("require","__dirname","__filename",code+"\n;return typeof createManyspace==='function' ? createManyspace : null;")(require,__dirname,__filename);
function render(node,blocks){
  let peak=0,finite=true,count=0,sidePeak=0;
  for(let b=0;b<blocks;++b){
    const l=new Float32Array(node.size),r=new Float32Array(node.size);
    node.onaudioprocess({outputBuffer:{length:node.size,getChannelData:i=>i ? r:l}});
    for(let i=0;i<node.size;++i){finite=finite && Number.isFinite(l[i]) && Number.isFinite(r[i]);peak=Math.max(peak,Math.abs(l[i]),Math.abs(r[i]));sidePeak=Math.max(sidePeak,Math.abs(l[i]-r[i]));if(Math.abs(l[i])>1e-6)count++;}
  }
  return {peak,finite,count,sidePeak};
}
(async()=>{
  for(const roomFloats of [6,7,17,18,null]) {
    const oldABI={_ms_max_frames:()=>4096,_ms_telemetry_floats:()=>68};
    if(roomFloats!==null)oldABI._ms_room_floats=()=>roomFloats;
    await assert.rejects(Host.create({loadCoreText:async()=>text,evaluate:()=>()=>oldABI}),/ABI mismatch/);
  }
  const telemetry=[];
  const host=await Host.create({loadCoreText:async()=>text,onTelemetry:t=>telemetry.push(t),evaluate:browserEvaluate});
  await assert.rejects(host.call("command",{kind:"leap"}),/not ready/);
  const custom=JSON.parse(JSON.stringify(scene64));custom.performance.source="custom";
  await assert.rejects(host.call("prepare",{scene:custom,task:"play"}),/Reselect/);
  const bad=JSON.parse(JSON.stringify(scene64));bad.transitions[3][0]+=.5;
  await assert.rejects(host.call("prepare",{scene:bad,task:"play"}),/probability/);
  const ready=await host.call("prepare",{scene:scene64,task:"play",preview:"room"});
  assert.equal(ready.rate,48000);assert.equal(processors.length,1);
  const node=processors[0];
  const silent=render(node,2);assert.equal(silent.count,0,"paused transport renders silence");assert.ok(silent.finite);
  await host.call("command",{kind:"bus",value:1});
  await host.call("command",{kind:"play",value:1});
  const playing=render(node,24);
  assert.ok(playing.finite && playing.peak>.01 && playing.peak<=1,"audible bounded output: peak "+playing.peak);
  let snap=host.snapshot();assert.equal(snap.playing,true);assert.equal(snap.room,scene64.performance.room);assert.ok(snap.seconds>1.9);
  await host.call("command",{kind:"leap"});render(node,1);
  snap=host.snapshot();assert.notEqual(snap.room,scene64.performance.room,"leap moves the destination");
  render(node,12);snap=host.snapshot();assert.ok(snap.voices.filter(v=>v.active).length>=2,"departing tail and new visit are both active");
  assert.ok(snap.peak>0 && snap.rate===48000 && snap.max_ms>=0 && snap.overruns===0);
  await host.call("command",{kind:"knobs",mix:.2,decay:1,sensitivity:.9});
  await host.call("command",{kind:"room",value:7});render(node,1);assert.equal(host.snapshot().room,7);
  await assert.rejects(host.call("command",{kind:"knobs",mix:2,decay:1,sensitivity:.9}),/Invalid Mix/);
  await assert.rejects(host.call("command",{kind:"bogus"}),/Unknown/);
  await host.call("command",{kind:"play",value:0});render(node,3);assert.equal(host.snapshot().playing,false);
  const stereo=JSON.parse(JSON.stringify(scene64));
  for(const spread of [0,1]) {
    stereo.rooms.forEach(r=>{r.spread=spread;r.pan=0;});
    await host.call("prepare",{scene:stereo,task:"play",preview:"room"});
    await host.call("command",{kind:"bus",value:2});await host.call("command",{kind:"play",value:1});
    render(node,2);const wet=render(node,24);
    assert.ok(wet.finite && wet.peak>1e-4);
    if(!spread)assert.ok(wet.sidePeak<2e-8,"0% Spread produces centered wet audio through the compiled browser core: "+wet.sidePeak);
    else assert.ok(wet.sidePeak>1e-4,"100% Spread retains stereo wet audio");
  }
  const badSpread=JSON.parse(JSON.stringify(scene64));badSpread.rooms[0].spread=1.01;
  await assert.rejects(host.call("prepare",{scene:badSpread,task:"play"}),/Spread/);
  for(const key of Object.keys(M.CHARACTER))for(const value of [null,NaN,M.LIMITS[key][1]+1]) {
    const bad=JSON.parse(JSON.stringify(scene64));bad.rooms[0][key]=value;
    await assert.rejects(host.call("prepare",{scene:bad,task:"play"}),/Invalid/);
  }
  const full=JSON.parse(JSON.stringify(scene64));
  full.rooms.forEach(r=>Object.assign(r,{diffusion:1,predelay_ms:120,reflections:7,modulation_ms:6,modulation_hz:1.5,
    shimmer:.4,hold:1,drive:6,ducking:.8,pan:.5,bloom_ms:600}));
  await host.call("prepare",{scene:full,task:"play"});await host.call("command",{kind:"play",value:1});
  for(let i=0;i<8;++i){await host.call("command",{kind:"next"});render(node,2);}
  const began=performance.now(),stress=render(node,48),elapsed=performance.now()-began;
  assert.ok(stress.finite && stress.peak>1e-5 && stress.peak<=1);
  assert.equal(host.snapshot().voices.filter(v=>v.active).length,8);
  console.log("WASM character stress: 48 x 4096 frames, eight visits, "+elapsed.toFixed(1)+" ms aggregate render/check time");
  // 16-room scene in Shape path mode re-prepares on the same context.
  const shaped=await host.call("prepare",{scene:scene16,task:"shape",preview:"path"});
  assert.equal(shaped.rate,48000);assert.equal(processors.length,1,"one processor node per host");
  await host.call("command",{kind:"play",value:1});const again=render(node,12);assert.ok(again.finite && again.peak>.01);
  assert.ok(telemetry.length>=2,"telemetry published on prepare");
  await host.close();
  console.log("wasm core: module "+(text.length/1024).toFixed(0)+" KB; prepare/command/process/telemetry through web/wasm-host.js OK");
})().catch(e=>{console.error(e);process.exit(1);});
