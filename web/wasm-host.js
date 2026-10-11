/* Browser audio host: runs the portable C++ core as WebAssembly inside the page.
   Mirrors the native Studio bridge (prepare/command/telemetry). No DSP in JavaScript.
   The Emscripten module is a single-file script evaluated with Function(), so no
   script URL has to pass Qollab's CSP. Processing uses ScriptProcessorNode by default;
   an AudioWorklet is attempted only when a module URL allowed by CSP is configured. */
var ManyspaceWasmHost = (() => {
  "use strict";
  const KIND = {knobs:0,next:1,leap:2,room:3,automatic:4,traversal:5,play:6,loop:7,bus:8,trigger:4};
  const EXAMPLE = {plucks:0,drums:1,chords:2};
  const T = {cursor:0,room:1,envelope:2,threshold:3,peakL:4,peakR:5,grain:6,playing:7,instances:8,frames:9,rate:10,loopFrames:11,passes:12,voices:13};
  const VOICE = 7, BUFFER = 4096;
  const number=(v,lo,hi,name)=>{if(typeof v!=="number" || !Number.isFinite(v) || v<lo || v>hi)throw new Error("Invalid "+name);return v;};
  const integer=(v,lo,hi,name)=>{const n=number(v,lo,hi,name);if(!Number.isInteger(n))throw new Error("Invalid "+name);return n;};
  function sceneArguments(scene,shape,path) {
    if(!scene || scene.format!=="manyspace.scene" || scene.version!==4)throw new Error("Unsupported scene format");
    const rooms=scene.rooms,rows=scene.transitions,p=scene.performance,count=rooms.length;
    if(count!==16 && count!==64 && count!==256 || rows.length!==count)throw new Error("Malformed scene snapshot");
    const roomFloats=new Float32Array(count*18),probabilities=new Float32Array(count*count);
    rooms.forEach((r,i)=>{
      if(r.hilbert_index!==i)throw new Error("Scene identity/order mismatch");
      if(r.mode!=="decay" && r.mode!=="swell")throw new Error("room mode");
      roomFloats.set([number(r.distance_ms,15,95,"reflection spacing")/1000,number(r.speed,.5,1.5,"speed"),number(r.damping,.2,.85,"damping"),
        r.mode==="swell" ? 1:0,number(r.lowpass_hz,800,16000,"High cut"),number(r.highpass_hz,10,1000,"Low cut"),number(r.spread,0,1,"Spread"),
        number(r.diffusion,0,1,"Diffusion"),number(r.predelay_ms,0,120,"Pre-delay")/1000,integer(r.reflections,0,7,"Early reflections"),
        number(r.modulation_ms,0,6,"Modulation depth")/1000,number(r.modulation_hz,.1,1.5,"Modulation rate"),
        number(r.shimmer,0,.4,"Shimmer"),number(r.hold,0,1,"Hold"),number(r.drive,1,6,"Loop drive"),
        number(r.ducking,0,.8,"Ducking"),number(r.pan,-1,1,"Pan"),number(r.bloom_ms,0,600,"Bloom")/1000],i*18);
      let sum=0;rows[i].forEach((x,j)=>{probabilities[i*count+j]=number(x,0,1,"probability");sum+=Number(x);});
      if(Math.abs(sum-1)>1e-6)throw new Error("Incomplete probability row");
    });
    if(!(p.source in EXAMPLE) && p.source!=="custom")throw new Error("Invalid performance settings.");
    return {count,roomFloats,probabilities,mix:number(p.mix,0,1,"Mix"),decay:number(p.decay,.3,8,"Decay"),sensitivity:number(p.sensitivity,0,1,"Sensitivity"),
      automatic:shape ? 0:integer({manual:0,auto:1,loop:2}[p.trigger ?? (p.automatic ? "auto":"manual")],0,2,"trigger"),traversal:p.traversal==="next" ? 0:1,budget:number(p.budget,4,8,"budget"),
      seed:number(p.playback_seed,0,4294967295,"playback seed"),example:p.source==="custom" ? 3:EXAMPLE[p.source],phraseNext:shape && path && p.source!=="custom" ? 1:0,
      loop:p.loop ? 1:0,room:number(p.room,0,count-1,"starting room")};
  }
  function evaluate(coreText) {
    // The module text defines createManyspace; evaluate it in an isolated function scope.
    return new Function(coreText+"\n;return typeof createManyspace==='function' ? createManyspace : null;")();
  }
  function instantiate(coreText,evaluator=evaluate) {
    const factory=evaluator(coreText);
    if(!factory)throw new Error("The C++ core module did not load.");
    return factory();
  }
  async function create(options) {
    const {loadCoreText,onTelemetry}=options;
    const text=await loadCoreText();
    if(typeof text!=="string" || text.length<1000)throw new Error("The C++ core module is missing from this project.");
    const m=await instantiate(text,options.evaluate);
    const maxFrames=m._ms_max_frames(),telemetryFloats=m._ms_telemetry_floats();
    if(maxFrames<BUFFER || telemetryFloats<T.voices+8*VOICE || typeof m._ms_room_floats!=="function" || m._ms_room_floats()!==18 || typeof m._ms_max_rooms!=="function" || m._ms_max_rooms()!==256 || ['_ms_source_ptr','_ms_source_max_frames','_ms_events','_ms_event_count','_ms_dropped_events'].some(k=>typeof m[k]!=="function"))throw new Error("Core module ABI mismatch");
    let custom=null,recording=null,lastTake=null,events=[];
    let context=null,node=null,prepared=false,frames=0,rate=48000,maxNanos=0,overruns=0,timer=null,loop=true,roomCount=0;
    const errorText=()=>m.UTF8ToString(m._ms_error());
    function ensureContext() {
      if(context)return context;
      context=new AudioContext();rate=context.sampleRate;
      if(!m._ms_create(rate))throw new Error(errorText());
      node=context.createScriptProcessor(BUFFER,1,2);
      node.onaudioprocess=event=>{
        const started=performance.now(),out=event.outputBuffer,n=out.length;
        const left=out.getChannelData(0),right=out.getChannelData(1);
        if(!prepared || !m._ms_process(n)){left.fill(0);right.fill(0);return;}
        left.set(m.HEAPF32.subarray(m._ms_left()>>2,(m._ms_left()>>2)+n));
        right.set(m.HEAPF32.subarray(m._ms_right()>>2,(m._ms_right()>>2)+n));
        if(recording) {
          const take=recording, keep=Math.min(n,rate*600-take.frames);
          if(keep>0) { take.left.push(left.slice(0,keep));take.right.push(right.slice(0,keep));take.frames+=keep; }
          if(take.frames>=rate*600)finishTake();
        }
        const count=m._ms_event_count(),ptr=m._ms_events()>>2;
        for(let i=0;i<count;++i){const d=ptr+i*3;events.push({from:m.HEAPF32[d],to:m.HEAPF32[d+1],reason:['manual','auto','loop end','by hand','path'][m.HEAPF32[d+2]]});}
        const ms=performance.now()-started;maxNanos=Math.max(maxNanos,ms*1e6);
        if(ms>1000*n/rate)overruns++;
      };
      node.connect(context.destination);
      return context;
    }
    function snapshot(drainEvents=false) {
      const t=m.HEAPF32.subarray(m._ms_telemetry()>>2,(m._ms_telemetry()>>2)+telemetryFloats);
      const voices=[];
      for(let i=0;i<8;++i){const d=T.voices+i*VOICE;voices.push({instance:t[d],room:t[d+1],age:t[d+2],gain:t[d+3],active:t[d+5]>0});}
      return {events:drainEvents?events.splice(0):events.slice(),dropped_events:m._ms_dropped_events(),passes:t[T.passes],instances:t[T.instances],recording:!!recording,take_seconds:(recording || lastTake)?.frames/rate || 0,room:t[T.room],playing:t[T.playing]>0,seconds:t[T.cursor]/rate,ended:!loop && t[T.cursor]>=frames && frames>0,
        peak:Math.max(t[T.peakL],t[T.peakR]),envelope:t[T.envelope],threshold:t[T.threshold],rate,max_ms:maxNanos/1e6,overruns,voices};
    }
    function finishTake(){if(recording){lastTake=recording;recording=null;}}
    function publish(){if(prepared && onTelemetry)onTelemetry(snapshot(true));}
    const host={
      kind:"wasm-scriptprocessor",
      async call(method,payload={}) {
        if(method==="prepare") {
          const a=sceneArguments(payload.scene,payload.task==="shape",payload.preview==="path");
          ensureContext();prepared=false;events=[];
          m.HEAPF32.set(a.roomFloats,m._ms_rooms()>>2);m.HEAPF32.set(a.probabilities,m._ms_probabilities()>>2);
          if(a.example===3) {
            if(!custom)throw new Error("Reselect the custom audio file.");
            const ptr=m._ms_source_ptr(custom.frames);
            if(!ptr)throw new Error(errorText());
            m.HEAPF32.set(custom.samples,ptr>>2);
          }
          if(!m._ms_prepare(a.count,a.mix,a.decay,a.sensitivity,a.automatic,a.traversal,a.budget,a.seed,a.example,0,a.phraseNext,a.loop,a.room))throw new Error(errorText());
          m._ms_process(0);frames=m.HEAPF32[(m._ms_telemetry()>>2)+T.frames];loop=!!a.loop;
          maxNanos=0;overruns=0;prepared=true;roomCount=a.count;
          if(!timer)timer=setInterval(publish,33);
          publish();return {rate};
        }
        if(method==="command") {
          if(!prepared)throw new Error("Audio is not ready");
          const k=payload.kind;let a=0,b=0,c=0;
          if(k==="knobs"){a=number(payload.mix,0,1,"Mix");b=number(payload.decay,.3,8,"Decay");c=number(payload.sensitivity,0,1,"Sensitivity");}
          else if(k==="play"){a=number(payload.value,0,1,"play");if(a && context.state!=="running")await context.resume();}
          else if(k==="room")a=integer(payload.value,0,roomCount-1,"room");
          else if(k==="bus")a=integer(payload.value,0,3,"bus");
          else if(k==="automatic" || k==="trigger")a=integer(payload.value,0,2,k);
          else if(k==="loop" || k==="traversal")a=integer(payload.value,0,1,k);
          else if(k!=="next" && k!=="leap")throw new Error("Unknown audio command");
          if(k==="loop")loop=!!a;
          if(!m._ms_command(KIND[k],a,b,c))throw new Error(errorText());
          return true;
        }
        if(method==="suspend") { if(context?.suspend)await context.suspend();return true; }
        if(method==="loadAudio") {
          if(payload.bytes?.byteLength>32*1024*1024)throw new Error("Choose an audio file up to 32 MB.");
          if(!payload.bytes)throw new Error("Choose an audio file first.");
          ensureContext();
          const decoded=await context.decodeAudioData(payload.bytes);
          const n=decoded.length;
          if(!n || n>m._ms_source_max_frames())throw new Error("Audio is too long. Choose a clip up to "+(m._ms_source_max_frames()/rate).toFixed(1)+" seconds.");
          const left=decoded.getChannelData(0),right=decoded.getChannelData(Math.min(1,decoded.numberOfChannels-1));
          const samples=new Float32Array(n*2);
          for(let i=0;i<n;++i){samples[2*i]=left[i];samples[2*i+1]=right[i];}
          custom={samples,frames:n};return {name:payload.name || "Custom audio",frames:n,rate};
        }
        if(method==="record") { ensureContext(); if(payload.value){ recording={left:[],right:[],frames:0};lastTake=null; } else finishTake(); return {seconds:lastTake ? lastTake.frames/rate:0}; }
        if(method==="take") { if(!lastTake || !lastTake.frames)throw new Error("Record a take first.");return encodeWav(lastTake,rate); }
        throw new Error("Unknown host request");
      },
      snapshot,
      async close(){if(timer)clearInterval(timer);timer=null;prepared=false;if(node)node.disconnect();if(context)await context.close();m._ms_destroy();}
    };
    return host;
  }
  function encodeWav(take,rate) {
    const bytes=new Uint8Array(44+take.frames*4),v=new DataView(bytes.buffer);
    const str=(at,s)=>{for(let i=0;i<s.length;i++)bytes[at+i]=s.charCodeAt(i);};
    str(0,"RIFF");v.setUint32(4,bytes.length-8,true);str(8,"WAVEfmt ");v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,2,true);
    v.setUint32(24,rate,true);v.setUint32(28,rate*4,true);v.setUint16(32,4,true);v.setUint16(34,16,true);str(36,"data");v.setUint32(40,take.frames*4,true);
    let at=44;take.left.forEach((left,b)=>{const right=take.right[b];for(let i=0;i<left.length;++i)for(const x of [left[i],right[i]]){const y=Math.max(-1,Math.min(1,Number.isFinite(x)?x:0));v.setInt16(at,Math.round(y*(y<0?32768:32767)),true);at+=2;}});
    return bytes;
  }
  return {create,sceneArguments,KIND,T,encodeWav};
})();
if (typeof module !== "undefined" && module.exports) module.exports=ManyspaceWasmHost;
