/* Portable artifact model. No audio processing, DOM, network or storage. */
var ManyspaceModel = (() => {
  "use strict";
  const Q = "manyspace.quantum-galaxy", S = "manyspace.scene";
  const ACOUSTIC = ["distance_ms","speed","damping"];
  const LIMITS = {distance_ms:[15,95], speed:[.5,1.5], damping:[.2,.85], lowpass_hz:[800,16000], highpass_hz:[10,1000],spread:[0,1],diffusion:[0,1],predelay_ms:[0,120],reflections:[0,7],modulation_ms:[0,6],modulation_hz:[.1,1.5],shimmer:[0,.4],hold:[0,1],drive:[1,6],ducking:[0,.8],pan:[-1,1],bloom_ms:[0,600]};
  const CHARACTER={diffusion:0,predelay_ms:0,reflections:0,modulation_ms:0,modulation_hz:.3,shimmer:0,hold:0,drive:1,ducking:0,pan:0,bloom_ms:0};
  const defaults = () => ({ranges:{...copy(LIMITS),lowpass_hz:[2500,8000],highpass_hz:[20,40],spread:[.35,1],diffusion:[.3,.8],predelay_ms:[0,35],reflections:[1,7],modulation_ms:[0,1.2],modulation_hz:[.15,.6],shimmer:[0,.12],hold:[0,0],drive:[1,1.3],ducking:[0,.35],pan:[-.6,.6],bloom_ms:[0,140]}, weights:[.5,.3,.2]});
  const performance = () => ({mix:.55, decay:2.8, sensitivity:.65, automatic:false, trigger:"manual",
    traversal:"leap", loop:true, budget:8, playback_seed:11, source:"plucks", room:0});
  function copy(x) { return JSON.parse(JSON.stringify(x)); }
  function require(ok, message) { if (!ok) throw new Error(message); }
  function number(x, lo, hi) { return typeof x === "number" && Number.isFinite(x) && x >= lo && x <= hi; }
  function integer(x, lo, hi) { return Number.isSafeInteger(x) && number(x,lo,hi); }
  function id(i) { return "room-"+String(i).padStart(2,"0"); }
  function matrix(p,n) {
    require(Array.isArray(p) && p.length === n,"Probability matrix has the wrong size.");
    p.forEach(row => {
      require(Array.isArray(row) && row.length === n && row.every(x=>number(x,0,1)),"Invalid probability row.");
      require(Math.abs(row.reduce((a,b)=>a+b,0)-1)<1e-8,"Probabilities must sum to one; uncovered rows cannot be invented.");
    });
  }
  function validateQ(q) {
    require(q && q.format === Q && q.version === 1,"Unsupported Quantum Galaxy format.");
    require(Array.isArray(q.rooms) && [16,64,256].includes(q.rooms.length),"A galaxy must contain 16, 64 or 256 rooms.");
    const n=q.rooms.length;
    q.rooms.forEach((r,i)=>require(r && r.id===id(i) && r.source===i && integer(r.outcome,0,63),"Invalid quantum room identity or outcome."));
    matrix(q.transitions,n); matrix(q.dephased,n);
    require(q.graph && q.graph.kind==="hypercube" && Array.isArray(q.graph.edges),"Missing quantum graph.");
    const expected=new Set();
    for(let i=0;i<n;i++) for(let bit=0;(1<<bit)<n;bit++) if(i<(i^(1<<bit))) expected.add([id(i),id(i^(1<<bit))].sort().join(":"));
    const edges=q.graph.edges.map(e=> {
      require(Array.isArray(e) && e.length===2 && e.every(x=>q.rooms.some(r=>r.id===x)),"Invalid graph edge.");
      return e.slice().sort().join(":");
    });
    require(edges.length===expected.size && new Set(edges).size===edges.length && edges.every(e=>expected.has(e)),"Graph does not match its source identities.");
    const g=q.generation;
    require(g && integer(g.seed,0,0xffffffff) && integer(g.depth,2,8) && number(g.phase,-100,100)
      && integer(g.shots,0,100000000) && typeof g.backend==="string" && g.backend.length>0,"Missing generation provenance.");
    require(q.circuits && ["properties","walk","single_job_candidate"].every(k=>typeof q.circuits[k]==="string" && q.circuits[k].startsWith("OPENQASM 2.0;")),"Missing circuit definitions.");
    require(q.provenance && typeof q.provenance.ai_assistance==="string" && typeof q.provenance.prior_concepts==="string","Missing provenance.");
    if (g.shots) require(Array.isArray(g.source_coverage) && g.source_coverage.length===n
      && g.source_coverage.every(x=>integer(x,1,g.shots)) && g.source_coverage.reduce((a,b)=>a+b,0)===g.shots,"Incomplete sampled-source coverage.");
    require(q.baseline_kind==="computed ideal dephased reference","Baseline provenance is missing.");
    const finite=value=>{
      if(typeof value==="number")require(Number.isFinite(value),"Nonfinite artifact data.");
      else if(value && typeof value==="object")Object.values(value).forEach(finite);
    };
    finite(q);
    return q;
  }
  function fromLegacy(g) {
    require(g && g.schema_version===1 && Array.isArray(g.rooms),"Unsupported saved galaxy.");
    const n=g.rooms.length, positions=Array.from({length:n},(_,i)=>i).sort((a,b)=>g.rooms[a].source-g.rooms[b].source);
    matrix(g.transitions,n);matrix(g.dephased,n);
    const q={format:Q,version:1,rooms:positions.map(i=>({id:g.rooms[i].id,source:g.rooms[i].source,outcome:g.rooms[i].property_outcome})),
      transitions:positions.map(i=>positions.map(j=>g.transitions[i][j])),
      dephased:positions.map(i=>positions.map(j=>g.dephased[i][j])),
      graph:copy(g.graph),generation:copy(g.generation),circuits:copy(g.circuits),provenance:copy(g.provenance),
      baseline_kind:"computed ideal dephased reference"};
    validateQ(q);
    require(g.rooms.every((r,i)=>r.hilbert_index===i && JSON.stringify(r.xy)===JSON.stringify(xy(Math.sqrt(n),i))),"Invalid saved Hilbert map.");
    g.rooms.forEach(r=> {
      const derived=derive(q.rooms[r.source],defaults());
      require(ACOUSTIC.every(k=>number(r[k],...LIMITS[k]) && Math.abs(r[k]-derived[k])<1e-8) && r.mode===derived.mode,"Saved properties do not match their quantum outcomes.");
    });
    return q;
  }
  function xy(side,index) {
    let x=0,y=0;
    for(let scale=1;scale<side;scale*=2) {
      const rx=(index>>1)&1,ry=(index^(index>>1))&1;
      if(!ry) { if(rx) {x=scale-1-x;y=scale-1-y;} [x,y]=[y,x]; }
      x+=scale*rx;y+=scale*ry;index>>=2;
    }
    return [x,y];
  }
  function validateMapping(m) {
    require(m && m.ranges && Object.keys(LIMITS).every(k=>Array.isArray(m.ranges[k]) && m.ranges[k].length===2
      && m.ranges[k].every(x=>number(x,...LIMITS[k]) && (k!=="reflections" || Number.isInteger(x))) && m.ranges[k][0]<=m.ranges[k][1]),"Mapping ranges exceed the engine's supported values.");
    require(Array.isArray(m.weights) && m.weights.length===3 && m.weights.every(x=>number(x,0,1))
      && Math.abs(m.weights.reduce((a,b)=>a+b,0)-1)<1e-8,"Ordering weights must sum to one.");
  }
  function derive(r,m) {
    const output={};
    ACOUSTIC.forEach((key,i)=> {const [lo,hi]=m.ranges[key];output[key]=lo+(hi-lo)*((r.outcome>>(2*i))&3)/3;});
    // Reuse measured bins for additional classical acoustic interpretation.
    // Greater damping softens the top; wider spacing retains more bass.
    const [lpLo,lpHi]=m.ranges.lowpass_hz,[hpLo,hpHi]=m.ranges.highpass_hz;
    output.lowpass_hz=lpHi-(lpHi-lpLo)*((r.outcome>>4)&3)/3;
    output.highpass_hz=hpHi-(hpHi-hpLo)*(r.outcome&3)/3;
    const [spreadLo,spreadHi]=m.ranges.spread;
    output.spread=spreadLo+(spreadHi-spreadLo)*(r.outcome&3)/3;
    // Existing measured bins plus stable source identity; no new quantum bits.
    const source=r.source||0,hash=((source*37+11)%64)/63;
    const bins={diffusion:((r.outcome>>4)&3)/3,predelay_ms:(r.outcome&3)/3,
      reflections:hash,modulation_ms:((r.outcome>>2)&3)/3,modulation_hz:(source%8)/7,
      shimmer:((r.outcome>>4)&3)/3,hold:source%8===0 ? 1:0,drive:hash,
      ducking:1-(r.outcome&3)/3,pan:xy(8,source)[0]/7,bloom_ms:1-((r.outcome>>2)&3)/3};
    Object.entries(bins).forEach(([key,t])=>{const [lo,hi]=m.ranges[key];output[key]=lo+(hi-lo)*t;});
    output.reflections=Math.round(output.reflections);
    let bits=r.outcome,pop=0; while(bits) {pop+=bits&1;bits>>=1;}
    output.mode=pop%2 ? "swell":"decay";return output;
  }
  function distance(a,b,weights) {
    return Math.sqrt(ACOUSTIC.reduce((sum,k,i)=>sum+weights[i]*Math.pow((a[k]-b[k])/(LIMITS[k][1]-LIMITS[k][0]),2),0));
  }
  function adjacency(rooms,weights) {
    const values=rooms.slice(1).map((r,i)=>distance(rooms[i],r,weights));
    return {mean:values.reduce((a,b)=>a+b,0)/values.length,max:Math.max(...values)};
  }
  function validatePerformance(p,n) {
    if (p && p.trigger===undefined) p.trigger=p.automatic ? "auto":"manual";
    require(p && ["manual","auto","loop"].includes(p.trigger),"Invalid trigger mode.");
    p.automatic=p.trigger==="auto";
    require(p && number(p.mix,0,1) && number(p.decay,.3,8) && number(p.sensitivity,0,1)
      && typeof p.automatic==="boolean" && typeof p.loop==="boolean" && ["next","leap"].includes(p.traversal)
      && [4,8].includes(p.budget) && integer(p.playback_seed,0,0xffffffff) && integer(p.room,0,n-1)
      && ["plucks","drums","chords","custom"].includes(p.source),"Invalid performance settings.");
  }
  function materialize(scene) {
    validateQ(scene.galaxy);validateMapping(scene.mapping);
    const q=scene.galaxy,n=q.rooms.length;
    require(Array.isArray(scene.order) && scene.order.length===n && new Set(scene.order).size===n
      && scene.order.every(name=>q.rooms.some(r=>r.id===name)),"Scene order must include each room exactly once.");
    require(scene.overrides && typeof scene.overrides==="object" && !Array.isArray(scene.overrides),"Invalid room edits.");
    Object.entries(scene.overrides).forEach(([name,fields])=>{
      require(q.rooms.some(r=>r.id===name) && fields && typeof fields==="object" && !Array.isArray(fields),"Unknown edited room.");
      Object.entries(fields).forEach(([key,value])=>require(key==="mode" ? ["decay","swell"].includes(value)
        : Object.hasOwn(LIMITS,key) && number(value,...LIMITS[key]) && (key!=="reflections" || Number.isInteger(value)),"Invalid room edit."));
    });
    validatePerformance(scene.performance,n);
    const rooms=scene.order.map((name,i)=>{
      const origin=q.rooms.find(r=>r.id===name),derived=derive(origin,scene.mapping);
      return {...origin,...derived,...scene.overrides[name],derived,edited:Object.keys(scene.overrides[name]||{}),
        hilbert_index:i,xy:xy(Math.sqrt(n),i)};
    });
    const sources=rooms.map(r=>r.source);
    require(rooms.every(r=>r.highpass_hz<r.lowpass_hz),"Low cut must be below High cut in every room.");
    scene.rooms=rooms;
    scene.transitions=sources.map(i=>sources.map(j=>q.transitions[i][j]));
    scene.dephased=sources.map(i=>sources.map(j=>q.dephased[i][j]));
    scene.evidence={before:adjacency(q.rooms.map(r=>({...derive(r,scene.mapping),...scene.overrides[r.id]})),scene.mapping.weights),
      after:adjacency(rooms,scene.mapping.weights)};
    return scene;
  }
  function arrange(scene) {
    materialize(scene);
    const rooms=scene.galaxy.rooms.map(r=>({...r,...derive(r,scene.mapping),...scene.overrides[r.id]}));
    const distances=rooms.map(a=>Float64Array.from(rooms,b=>distance(a,b,scene.mapping.weights)));
    let best,score=Infinity;
    for(let start=0;start<rooms.length;start++) {
      const path=[start],remaining=new Set(rooms.map((_,i)=>i));remaining.delete(start);
      while(remaining.size) {
        const current=path[path.length-1];
        let next,minimum=Infinity;
        for(const i of remaining) {
          const d=distances[current][i];
          if(d<minimum) {minimum=d;next=i;}
        }
        remaining.delete(next);path.push(next);
      }
      const value=adjacency(path.map(i=>rooms[i]),scene.mapping.weights).mean;
      if(value<score) {score=value;best=path;}
    }
    const currentID=scene.rooms[scene.performance.room].id;
    scene.order=best.map(i=>rooms[i].id);
    scene.performance.room=scene.order.indexOf(currentID);
    return materialize(scene);
  }
  function createScene(q,name="Untitled scene") {
    validateQ(q);
    const s={format:S,version:4,name,galaxy:copy(q),mapping:defaults(),order:q.rooms.map(r=>r.id),
      overrides:{},performance:performance()};
    return arrange(s);
  }
  function load(input) {
    if(input && input.schema_version===1) {
      const q=fromLegacy(input),s=createScene(q,"Reference galaxy");
      s.order=input.rooms.map(r=>r.id);s.performance.playback_seed=input.runtime.playback_seed;
      return {galaxy:q,scene:materialize(s)};
    }
    if(input && input.format===Q) {const q=copy(validateQ(input));return {galaxy:q,scene:createScene(q)};}
    require(input && input.format===S && [1,2,3,4].includes(input.version) && typeof input.name==="string" && input.name.length<=120,"Unsupported Manyspace Scene.");
    const s=copy(input);
    if(input.version<4) {
      require(s.mapping && s.mapping.ranges && s.overrides && !Array.isArray(s.overrides),"Invalid legacy scene mapping or edits.");
      const allowed=input.version===1 ? ACOUSTIC:[...ACOUSTIC,"lowpass_hz","highpass_hz",...(input.version===3 ? ["spread"]:[])];
      require(Object.keys(s.mapping.ranges).every(k=>allowed.includes(k)) && Object.values(s.overrides).every(fields=>
        fields && typeof fields==="object" && Object.keys(fields).every(k=>allowed.includes(k) || k==="mode")),"Legacy scenes cannot contain newer room parameters.");
    }
    if(input.version===1) {
      s.mapping.ranges={...s.mapping.ranges,lowpass_hz:defaults().ranges.lowpass_hz,highpass_hz:defaults().ranges.highpass_hz};
    }
    if(input.version<3) {
      // Preserve the old full-width sound; fresh galaxies use varied defaults.
      s.mapping.ranges.spread=[1,1];s.version=3;
    }
    if(input.version<4){Object.entries(CHARACTER).forEach(([k,v])=>s.mapping.ranges[k]=[v,v]);s.version=4;}
    const expected=materialize(copy(s));
    const expectedRooms=copy(expected.rooms);
    if(input.version<4)expectedRooms.forEach(r=>Object.keys(CHARACTER).forEach(k=>{delete r[k];delete r.derived[k];}));
    if(input.version<3)expectedRooms.forEach(r=> {delete r.spread;delete r.derived.spread;});
    if(input.version===1)expectedRooms.forEach(r=> {
      delete r.lowpass_hz;delete r.highpass_hz;delete r.derived.lowpass_hz;delete r.derived.highpass_hz;
    });
    if(input.rooms) require(JSON.stringify(input.rooms)===JSON.stringify(expectedRooms),"Scene room data is inconsistent with its origin, mapping or edits.");
    if(input.transitions) require(JSON.stringify(input.transitions)===JSON.stringify(expected.transitions),"Scene probabilities do not match its room order.");
    if(input.dephased) require(JSON.stringify(input.dephased)===JSON.stringify(expected.dephased),"Scene baseline does not match its room order.");
    return {galaxy:copy(expected.galaxy),scene:materialize(s)};
  }
  function edit(scene,name,key,value) {
    const s=copy(scene);s.overrides[name]={...(s.overrides[name]||{}),[key]:value};return materialize(s);
  }
  function restore(scene,name) {const s=copy(scene);delete s.overrides[name];return materialize(s);}
  function serialize(value) {
    const valid=value.format===S ? load(value).scene:validateQ(value);
    let text=JSON.stringify(valid,null,2);
    if(new TextEncoder().encode(text).length>2*1024*1024)text=JSON.stringify(valid);
    require(new TextEncoder().encode(text).length<=2*1024*1024,"Artifact exceeds the 2 MB project-file limit.");
    return text;
  }
  function tv(a,b) {return a.reduce((sum,p,i)=>sum+Math.abs(p-b[i]),0)/2;}
  return {Q,S,LIMITS,CHARACTER,copy,defaults,performance,validateQ,fromLegacy,xy,derive,distance,adjacency,
    materialize,arrange,createScene,load,edit,restore,serialize,tv};
})();
if (typeof module !== "undefined" && module.exports) module.exports=ManyspaceModel;
