'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),Host=require('../web/wasm-host.js'),M=require('../web/model.js');
const text=fs.readFileSync('build/wasm/manyspace-core.js','utf8');
let node, stereoInput=false;
class AudioContext {
 constructor(){this.sampleRate=48000;this.state='suspended';this.destination={};}
 createScriptProcessor(n){node={connect(){},disconnect(){},n};return node;}
 async decodeAudioData(bytes){assert.ok(bytes.byteLength);return {length:1000,numberOfChannels:stereoInput?2:1,getChannelData:ch=>Float32Array.from({length:1000},(_,i)=>.2*Math.sin(i*.1)*(stereoInput && ch===1?-.5:1))};}
 async resume(){this.state='running';}async suspend(){this.state='suspended';}async close(){}
}
global.AudioContext=AudioContext;
const evaluate=code=>new Function('require','__dirname','__filename',code+';return createManyspace;')(require,__dirname,__filename);
const render=()=>{let channels=[new Float32Array(4096),new Float32Array(4096)];node.onaudioprocess({outputBuffer:{length:4096,getChannelData:i=>channels[i]}});return channels;};
(async()=>{
 const h=await Host.create({loadCoreText:async()=>text,evaluate});
 const s=M.load(JSON.parse(fs.readFileSync('build/workflow/scene16.json','utf8'))).scene;s.performance.source='custom';s.performance.trigger='loop';s.performance.traversal='next';s.performance.room=0;
 await assert.rejects(h.call('prepare',{scene:s}),/Reselect/);
 await h.call('loadAudio',{bytes:new ArrayBuffer(44),name:'mono.wav'});
 await h.call('prepare',{scene:s,task:'play'});await h.call('command',{kind:'play',value:1});
 const stereo=render();assert.ok(stereo[0].some(v=>v!==0));assert.equal(h.snapshot().passes,4);assert.equal(h.snapshot().room,4);assert.deepEqual(h.snapshot().events.map(e=>[e.from,e.to,e.reason]),[[0,1,'loop end'],[1,2,'loop end'],[2,3,'loop end'],[3,4,'loop end']]);assert.equal(h.snapshot().events.length,4,'reading recording metadata cannot discard room changes');
 await h.call('record',{value:true});const output=render();await h.call('record',{value:false});const wav=await h.call('take');const v=new DataView(wav.buffer);
 assert.equal(Buffer.from(wav.subarray(0,4)).toString(),'RIFF');assert.equal(v.getUint32(24,true),48000);assert.equal(v.getUint32(40,true),4096*4);
 for(const i of [0,10,100,2000,4095])for(let ch=0;ch<2;ch++){const x=Math.max(-1,Math.min(1,output[ch][i]));assert.equal(v.getInt16(44+i*4+ch*2,true),Math.round(x*(x<0?32768:32767)),'take contains processed stereo output');}
 s.performance.loop=false;s.performance.room=0;await h.call('prepare',{scene:s,task:'play'});await h.call('command',{kind:'play',value:1});render();assert.equal(h.snapshot().passes,1);assert.equal(h.snapshot().room,1);render();assert.equal(h.snapshot().passes,1);
 await assert.rejects(h.call('command',{kind:'trigger',value:3}),/Invalid/);
 await h.call('prepare',{scene:s,task:'shape',preview:'path'});await h.call('command',{kind:'play',value:1});render();assert.equal(h.snapshot().room,0,'custom path disables scheduling and loop traversal in Shape');
 stereoInput=true;await h.call('loadAudio',{bytes:new ArrayBuffer(44),name:'stereo.wav'});await h.call('prepare',{scene:s,task:'play'});await h.call('command',{kind:'bus',value:0});await h.call('command',{kind:'play',value:1});const dry=render();for(const i of [400,600,900])assert.ok(Math.abs(dry[1][i]+dry[0][i]*.5)<1e-7,'stereo input channels preserved in the dry bus');
 // Exercise the new blend through the actual JS → WASM command path.
 s.performance.source='plucks';s.performance.trigger='manual';s.performance.loop=true;s.performance.room=0;
 async function blend(mix,bus){s.performance.mix=mix;await h.call('prepare',{scene:s,task:'play'});await h.call('command',{kind:'bus',value:bus});await h.call('command',{kind:'play',value:1});let output;for(let i=0;i<6;++i)output=render();return output;}
 const dryReference=await blend(1,0),wetReference=await blend(1,2);
 for(const mix of [0,.5,1]){const actual=await blend(mix,3);for(let ch=0;ch<2;++ch)for(let i=0;i<4096;++i){const expected=(1-mix)*dryReference[ch][i]+4*mix*wetReference[ch][i];assert.ok(Math.abs(actual[ch][i]-expected)<1e-6,'Immersion curve reaches compiled stereo output');}}
 await assert.rejects(h.call('command',{kind:'bus',value:4}),/Invalid/);
 await h.close();console.log('notebook audio: compiled WASM custom mono/stereo, exact Each loop/Once, Shape exclusion and PCM take content pass');
})().catch(e=>{console.error(e);process.exit(1)});
