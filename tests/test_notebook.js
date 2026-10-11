'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const N=require('../web/notebook.js'),M=require('../web/model.js');
const room={distance_ms:68,damping:.63,speed:.5,mode:'swell',spread:.35,lowpass_hz:2500};
assert.equal(N.words(room).name,'a wide, dark room');assert.match(N.words(room).facts,/walls 68 ms apart.*0.50×.*63% damped.*reversed tails.*35% spread.*2500 Hz/);
for(const [distance_ms,size] of [[34.9,'tight'],[35,'small'],[55,'wide'],[75,'vast']])assert.match(N.words({...room,distance_ms}).name,new RegExp(size));
for(const [damping,tone] of [[.2,'bright'],[.35,'warm'],[.55,'dark'],[.72,'muffled']])assert.match(N.words({...room,damping,lowpass_hz:8000}).name,new RegExp(tone));
assert.match(N.words({...room,damping:.2,lowpass_hz:800}).name,/dark/);
function step(n,auto=false){return {dataset:{step:String(n)},inert:false,classList:{toggle(){}},hasAttribute:k=>k==='data-auto' && auto,closest(){return this;}};}
const list=[step(1),step(3),step(4),step(5),step(2,true),step(6,true),step(7,true),step(8)];
const g=N.gate({querySelectorAll:()=>list});
assert.deepEqual(g.state,['active','locked','locked','locked','locked','locked','locked','locked']);assert.equal(list[4].inert,true);
g.done(7);assert.equal(g.state[7],'locked','locked clicks cannot skip the procedure');
g.complete(list[0]);assert.deepEqual(g.state.slice(0,3),['done','done','active'],'cross-page auto step unlocks in numerical order');assert.equal(list[4].inert,false);
g.complete(list[1]);g.complete(list[2]);g.complete(list[3]);assert.deepEqual(g.state,['done','done','done','done','done','done','done','active']);
const context=vm.createContext({});vm.runInContext(fs.readFileSync('web/notebook-templates.js','utf8'),context);const pages=context.ManyspaceNotebookPages;
assert.deepEqual(Object.keys(pages),['cover','generate','shape','play','appendix']);
const welcome='Welcome to Manyfold. One room, many variations. Hear how the same sound changes from room to room, with echoes lingering after each leap. Start with the ready-made example, or generate and shape your own.';
const definition='A manyfold is a collection of room variations and the probabilities connecting them.';
assert.ok(pages.cover.includes('Greetings, sonic adventurer!'));
assert.ok(pages.cover.includes(welcome));assert.equal(pages.cover.split(definition).length,2);
assert.match(pages.cover,/Play Manyfold/);assert.match(pages.cover,/A ready-made manyfold is available to try/);
assert.match(pages.generate,/MANYFOLD READY/);assert.match(pages.generate,/On a simulator that uses a seed, changing it can change the sampled results/);
assert.match(pages.shape,/Which manyfold\?/);assert.match(pages.play,/Explore with sound/);
for(const html of Object.values(pages))assert.doesNotMatch(html,/multi-verb|multiverb|Manyfolds|galaxies|bubbles/i);
for(const html of Object.values(pages)){assert.doesNotMatch(html,/Sketch\.mount|sketch-head|class="note"|href="03b-|<script|googleapis/);}
assert.match(pages.play,/Linger/);assert.doesNotMatch(pages.play,/Decay|spoken voice|bass riff/);assert.match(pages.generate,/value="3" data-role="depth/);assert.match(pages.generate,/value="0.73" data-role="phase/);assert.doesNotMatch(pages.generate,/name="g-size"[^>]*checked/);assert.doesNotMatch(pages.play,/data-role="(?:step|leap|effect)"[^>]*checked/);
const legacy=M.load(JSON.parse(fs.readFileSync('build/workflow/scene.json','utf8'))).scene;
for(const automatic of [false,true]){const s=M.copy(legacy);delete s.performance.trigger;s.performance.automatic=automatic;assert.equal(M.load(s).scene.performance.trigger,automatic?'auto':'manual');}
const loop=M.copy(legacy);loop.performance.trigger='loop';assert.equal(M.load(loop).scene.performance.automatic,false);assert.equal(M.load(JSON.parse(M.serialize(loop))).scene.performance.trigger,'loop');
loop.performance.trigger='bad';assert.throws(()=>M.load(loop),/trigger/);
// Render precision cannot become serialization precision, including legacy imports.
vm.runInContext(fs.readFileSync('web/notebook-parameters.js','utf8'),context);
const percentages=new Set(['spread','diffusion','shimmer','hold','ducking']);
assert.equal(N.displayNumber(.8499999999999999,2),'0.85');
assert.equal(N.displayNumber(.15000000000000002,2),'0.15');
assert.equal(N.displayNumber(35.00000000000001,0),'35');
assert.equal(N.displayNumber(-.6000000000000001,2),'-0.6');
for(const count of [16,64]){
 const scene=M.load(JSON.parse(fs.readFileSync(`presets/reference${count}.json`,'utf8'))).scene;
 const original=M.serialize(scene);
 for(const r of scene.rooms)for(const [key,p] of Object.entries(context.ManyspaceNotebookParameters))N.displayNumber(r[key]*(percentages.has(key)?100:1),p[7]);
 assert.equal(M.serialize(scene),original,'display formatting leaves every exported bit unchanged');
 const restored=M.load(JSON.parse(original)).scene;
 assert.deepEqual(restored.rooms,scene.rooms,'legacy import/export retains numeric room values');
 assert.deepEqual(restored.mapping,scene.mapping);
 assert.deepEqual(restored.transitions,scene.transitions);
}
console.log('notebook: approved copy, defaults, strict cross-page gating, words and trigger migration pass');
const fontCss=fs.readFileSync('web/notebook-fonts.css','utf8');
for(const slug of ['architects-daughter','permanent-marker']) {
 const bytes=fs.readFileSync('web/fonts/'+slug+'.woff2');assert.equal(bytes.subarray(0,4).toString(),'wOF2');assert.ok(fontCss.includes(bytes.toString('base64')),'font embedded byte for byte');
 const license=fs.readFileSync('web/fonts/'+slug+'-LICENSE.txt','utf8');assert.match(license,slug==='architects-daughter'?/OPEN FONT LICENSE/:/Apache License/);
}
assert.doesNotMatch(fontCss+fs.readFileSync('web/notebook.css','utf8'),/@import|url\(https?:/,'no runtime font dependency');
// A slider must update sound on input, without waiting for blur/mouse release.
(async()=>{
 function input(value){return {value,min:'0',max:'100',listeners:{},addEventListener(k,f){this.listeners[k]=f;}};}
 const range=input('55'),numeric=input('55'),commits=[];
 N.bindPair(range,numeric,async v=>commits.push(v));
 range.value='100';await range.listeners.input({target:range});assert.equal(numeric.value,'100');assert.deepEqual(commits,[100]);
 numeric.value='';await numeric.listeners.input({target:numeric});assert.deepEqual(commits,[100],'incomplete numeric edits do not commit');
 numeric.value='23';await numeric.listeners.input({target:numeric});assert.equal(range.value,'23');assert.deepEqual(commits,[100,23]);
 numeric.value='23.00';await numeric.listeners.input({target:numeric});assert.equal(numeric.value,'23.00','active typing is not normalized or replaced');
 await numeric.listeners.change({target:numeric});assert.deepEqual(commits,[100,23]);
 numeric.value='';await assert.rejects(numeric.listeners.change({target:numeric}),/finite/);
 numeric.value='101';await numeric.listeners.input({target:numeric});assert.equal(numeric.value,'101','out-of-range active input stays untouched');
 await numeric.listeners.change({target:numeric});assert.equal(numeric.value,'100','boundary normalization retains prior clamp behavior');
 const fineRange=input('0.15'),fineNumber=input('0.15'),fineCommits=[];
 fineRange.min='0.1';fineRange.max='1.5';fineRange.step='0.05';
 N.bindPair(fineRange,fineNumber,async v=>fineCommits.push(v));
 fineRange.value='0.15000000000000002';await fineRange.listeners.input({target:fineRange});
 assert.equal(fineNumber.value,'0.15');assert.equal(fineCommits[0],.15000000000000002,'display precision never rounds committed values');
 const retryRange=input('55'),retryNumber=input('55');let attempts=0;N.bindPair(retryRange,retryNumber,async()=>{if(++attempts===1)throw new Error('queue full');});retryRange.value='100';await assert.rejects(retryRange.listeners.input({target:retryRange}),/queue full/);await retryRange.listeners.input({target:retryRange});assert.equal(attempts,2,'failed commands can be retried at the same value');
 console.log('paired controls: live range/number updates, duplicate suppression and empty-value validation pass');
})().catch(e=>{console.error(e);process.exitCode=1;});

// SVG lines do not implement HTMLElement.hidden; visibility must use the attribute.
{
 global.ManyspaceModel=M;global.ManyspaceNotebookParameters=context.ManyspaceNotebookParameters;
 const scene=M.load(JSON.parse(fs.readFileSync('presets/reference16.json','utf8'))).scene;
 const attributes=new Map(),line={toggleAttribute(k,on){if(on)attributes.set(k,'');else attributes.delete(k);},setAttribute(k,v){attributes.set(k,v);}};
 const grid={style:{setProperty(){}},children:scene.rooms.map(()=>({style:{},dataset:{},setAttribute(){},innerHTML:''}))};
 const root={querySelector:s=>s==='[data-role="grid"]'?grid:s==='[data-role="arc"]'?line:null};
 N.drawMap(root,scene,3,'speed',[],[0,3]);
 assert.equal(attributes.has('hidden'),false,'last leap is visible');
 assert.equal(attributes.get('x2'),100,'leap ends at room 3 on the Hilbert map');
 N.drawMap(root,scene,3,'speed',[],null);
 assert.equal(attributes.has('hidden'),true,'clearing a leap hides the actual SVG element');
 N.drawMap(root,scene,4,'speed',[],null);
 assert.equal(attributes.has('hidden'),true,'next steps keep the old leap hidden');
 N.drawMap(root,scene,7,'speed',[],[4,7]);
 assert.equal(attributes.has('hidden'),false,'a fresh leap can display again');
 delete global.ManyspaceModel;delete global.ManyspaceNotebookParameters;
 console.log('notebook map: SVG leap visibility clears and restores correctly');
}
