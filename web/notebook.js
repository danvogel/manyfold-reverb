/* Qollab notebook presenter. Approved copy lives in notebook-templates/parameters.
   Scene interpretation is classical; audio and traversal run in the C++ host. */
var ManyspaceNotebook = (() => {
  'use strict';
  const esc=x=>String(x).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  function words(r,defaults={spread:1,lowpass_hz:8000}) {
    const size=r.distance_ms<35?'tight':r.distance_ms<55?'small':r.distance_ms<75?'wide':'vast';
    // A strongly closed high cut resolves otherwise bright/warm damping into dark tone.
    const tone=r.damping<.35 && r.lowpass_hz>=2500?'bright':r.damping<.55 && r.lowpass_hz>=1500?'warm':r.damping<.72?'dark':'muffled';
    let facts=`walls ${Math.round(r.distance_ms)} ms apart · plays at ${r.speed.toFixed(2)}× · ${Math.round(r.damping*100)}% damped · ${r.mode==='swell'?'reversed':'forward'} tails`;
    if(r.spread!==defaults.spread)facts+=` · ${Math.round(r.spread*100)}% spread`;
    if(r.lowpass_hz!==defaults.lowpass_hz)facts+=` · high cut ${Math.round(r.lowpass_hz)} Hz`;
    return {name:`a ${size}, ${tone} room`,facts};
  }
  function gate(root,onChange=()=>{}) {
    const list=[...root.querySelectorAll('[data-step]')].sort((a,b)=>Number(a.dataset.step)-Number(b.dataset.step));
    const state=list.map(()=> 'locked');state[0]='active';
    function paint(){list.forEach((e,i)=>{e.dataset.state=state[i];e.classList.toggle('locked',state[i]==='locked');e.classList.toggle('active',state[i]==='active');e.classList.toggle('done',state[i]==='done');e.inert=state[i]==='locked';e.setAttribute?.('role','group');e.setAttribute?.('aria-label',`Step ${e.dataset.step}: ${e.querySelector?.('h2')?.textContent || ''} (${state[i]})`);});onChange(state);}
    function done(i){if(state[i]!=='active')return;state[i]='done';if(i+1<state.length)state[i+1]='active';paint();if(list[i+1]?.hasAttribute('data-auto'))done(i+1);}
    paint();return {state,list,done,complete(el){done(list.indexOf(el.closest('[data-step]')));}};
  }
  function drawMap(root,scene,selected,key='speed',voices=[],arc=null,mini=false) {
    const grid=root.querySelector('[data-role="grid"]') || root.querySelector('#mini');if(!grid)return;
    const side=Math.sqrt(scene.rooms.length),centers=scene.rooms.map((r,i)=>{const [x,y]=ManyspaceModel.xy(side,i);return [(x+.5)*800/side,(y+.5)*800/side];});
    grid.style.setProperty('--side',side);
    const P=ManyspaceNotebookParameters;
    if(grid.children.length!==scene.rooms.length)grid.replaceChildren(...scene.rooms.map(()=>document.createElement(mini?'span':'button')));
    scene.rooms.forEach((r,i)=>{
      const cell=grid.children[i],[x,y]=ManyspaceModel.xy(side,i),limit=ManyspaceModel.LIMITS[key] || [0,1];
      const v=key==='mode'?(r.mode==='swell'?1:0):r[key],f=(v-limit[0])/(limit[1]-limit[0]);
      const tail=voices.filter(t=>t.active && t.room===i).reduce((m,t)=>Math.max(m,t.gain),0);
      const color=r.edited.length?`rgba(176,122,0,${.12+.55*f})`:`rgba(31,95,168,${.08+.55*f})`;
      const value=key==='mode'?(r.mode==='swell'?'reverse':'forward'):format(key,v,P);
      cell.className='cell '+(r.edited.length?'edited':'');
      if(mini)cell.setAttribute('aria-hidden','true');else {
        cell.type='button';cell.dataset.room=i;cell.setAttribute('aria-pressed',i===selected);
        cell.setAttribute('aria-label',`Room ${i}: ${value}`);cell.title=value;
      }
      cell.style.cssText=`grid-column:${x+1};grid-row:${y+1};background:${mini||voices.length?i===selected?'#dbe9ff':tail?`rgba(42,38,34,${.1+.5*tail})`:'#fff':color};${i===selected?'outline:2px solid #b03934;outline-offset:-2px;':''}`;
      const label=mini?'':String(i).padStart(2,'0')+(voices.length?'':`<small>${esc(value)}</small>`);
      if(cell.innerHTML!==label)cell.innerHTML=label;
    });
    root.querySelector('[data-role="path"]')?.setAttribute('points',centers.map(c=>c.join(',')).join(' '));
    const line=root.querySelector('[data-role="arc"]');if(line){line.toggleAttribute('hidden',!arc);if(arc){const a=centers[arc[0]],b=centers[arc[1]];line.setAttribute('x1',a[0]);line.setAttribute('y1',a[1]);line.setAttribute('x2',b[0]);line.setAttribute('y2',b[1]);}}
  }
  const percent=new Set(['spread','diffusion','shimmer','hold','ducking']);
  const unitValue=(key,v)=>percent.has(key)?v*100:v;
  const modelValue=(key,v)=>percent.has(key)?v/100:v;
  // Presentation only: never feed a rounded display back into the scene model.
  const displayNumber=(value,precision)=>String(Number(Number(value).toFixed(precision)));
  const stepPrecision=step=>{const s=String(step);return s.includes('.')?s.split('.')[1].length:0;};
  function format(key,v,P){if(key==='reflections')return ['off','corridor','even','cluster','scatter','pairs','wide','room'][v];const p=P[key];return p?unitValue(key,v).toFixed(p[7])+(p[1]?' '+p[1]:''):String(v);}
  // Both controls apply live; incomplete/out-of-range numeric edits wait for change.
  function bindPair(a,b,commit,wrap=f=>f) {
    let committed=+a.value;
    const apply=wrap(async e=>{
      if(e.target.value.trim()==='')throw new Error('Choose a finite value.');
      const v=Math.min(+a.max,Math.max(+a.min,+e.target.value));
      if(!Number.isFinite(v))throw new Error('Choose a finite value.');
      a.value=String(v);
      // Preserve the text/caret of active numeric input, including trailing decimals.
      if(e.target===a || v!==+e.target.value)b.value=displayNumber(v,stepPrecision(a.step || '1'));
      if(v!==committed){committed=v;try{await commit?.(v);}catch(error){if(committed===v)committed=NaN;throw error;}}
    });
    a.addEventListener('input',apply);
    b.addEventListener('input',e=>{const v=+b.value;if(b.value.trim()!=='' && Number.isFinite(v) && v>=+a.min && v<=+a.max && b.validity?.valid!==false)return apply(e);});
    b.addEventListener('change',apply);
  }
  async function mount(options={}) {
    const M=ManyspaceModel,P=ManyspaceNotebookParameters,root=document.getElementById('app');
    document.title='Manyfold';
    const workflow=options.workflow,backendHandle=options.backend;
    let reference=options.reference || window.ManyspaceReference;
    const unpack=x=>x?.toJs?x.toJs({dict_converter:Object.fromEntries}):x;
    if(!reference)reference=unpack(await workflow.bundled_reference.callPromising());
    const initial=M.load(reference).scene;
    let scene=initial,page='cover',selected=0,host=null,hostPromise=null,prepared=false,playing=false,preview='room',effect=false,lifted=false,kindChosen=false,usedBackend=false,busy=false,telemetry=null,arc=null,pendingReason=null,lastInstance=null,recording=false,takeNumber=0;
    let preparation=Promise.resolve(),generationResult=null;
    const log=[],start=performance.now(),pages={};
    root.className='notebook-shell';root.innerHTML='<nav class="nb"><div class="tabs" aria-label="Notebook pages">'+Object.entries({cover:'Cover',generate:'A · Generate',shape:'B · Shape',play:'C · Play',appendix:'Appendix'}).map(([k,label])=>`<button class="tab" data-nav="${k}" aria-current="${k==='cover'?'page':'false'}">${label}</button>`).join('')+'</div></nav>'+Object.entries(ManyspaceNotebookPages).map(([k,html])=>`<div data-page="${k}" ${k==='cover'?'':'hidden'}>${html}</div>`).join('')+'<p class="nb status" role="status" aria-live="polite"></p>';
    Object.keys(ManyspaceNotebookPages).forEach(k=>pages[k]=root.querySelector(`[data-page="${k}"]`));
    const el=(p,r)=>pages[p].querySelector(`[data-role="${r}"]`);
    root.querySelectorAll('.page').forEach(e=>e.dataset.art='paper-graph');root.querySelectorAll('.tape').forEach((e,i)=>{e.dataset.art='tape-'+(i%3+1);e.setAttribute('aria-hidden','true');});root.querySelectorAll('[data-onion]').forEach(e=>e.dataset.art='paper-tracing');root.querySelectorAll('.tick').forEach(e=>{e.dataset.art='tick-done';e.setAttribute('aria-hidden','true');});root.querySelectorAll('.polaroid').forEach(e=>e.dataset.art='polaroid-frame');el('generate','stamp').dataset.art='stamp-ready';pages.cover.querySelector('.stamp').dataset.art='stamp-open-me';pages.cover.querySelector('.sticker').dataset.art='card-index';pages.cover.querySelector('h1').dataset.art='letter-title';root.querySelectorAll('.zone').forEach(e=>e.setAttribute('aria-hidden','true'));[...pages.cover.querySelectorAll('.zone')].forEach((e,i)=>e.dataset.art=['photo-bench','fig-circuit','fig-mapping'][i]);pages.generate.querySelector('[data-art="fig-hypercube"]').dataset.art='fig-hypercube';pages.generate.querySelector('.circuit').dataset.art='fig-circuit';pages.play.querySelector('.zone').dataset.art='doodle-cassette';
    if(typeof ManyspaceNotebookArt !== 'undefined')ManyspaceNotebookArt.apply(pages);
    const status=message=>root.querySelector('.status').textContent=message;
    const report=e=>status(e.message || String(e));
    const act=fn=>event=>Promise.resolve().then(()=>fn(event)).catch(report);
    const listen=(p,r,type,fn)=>el(p,r)?.addEventListener(type,act(fn));
    const add=text=>{const t=(performance.now()-start)/1000;log.push(`${String(Math.floor(t/60)).padStart(2,'0')}:${String(Math.floor(t%60)).padStart(2,'0')}  ${text}`);el('play','log').value+=(el('play','log').value?'\n':'')+log.at(-1);};
    const gates={};
    ['generate','shape','play'].forEach(k=>{gates[k]=gate(pages[k],s=>{
      pages[k].querySelectorAll('[data-check] span').forEach((c,i)=>{c.textContent=(s[i]==='done'?'☑ ':'☐ ')+c.textContent.slice(2);c.classList.toggle('on',s[i]==='done');});
    });});
    const complete=(p,e)=>gates[p].complete(e);
    // Gating is committed after successful actions; failed uploads/jobs never unlock observations.
    ['generate','shape','play'].forEach(p=>pages[p].addEventListener('change',e=>{
      if(e.target.type==='file' || e.target.dataset.role==='effect' || (p==='shape' && e.target.closest('[data-step]')?.dataset.step==='1'))return;
      if(e.target.matches('input,select,textarea'))complete(p,e.target);
    }));
    // Selecting an already-selected radio or touching a default slider is still a choice.
    ['generate','shape','play'].forEach(p=>{const touch=e=>{if(e.type==='keydown' && ![' ','Space','Enter'].includes(e.key))return;
      if(!e.target.matches('input:not([type="file"]):not([type="checkbox"]),select,textarea,summary'))return;
      if(p==='shape' && e.target.closest('[data-step]')?.dataset.step==='1')return;
      complete(p,e.target);
    };pages[p].addEventListener('click',touch);pages[p].addEventListener('keydown',touch);});
    root.querySelectorAll('[data-skip]').forEach(b=>b.addEventListener('click',()=>complete(b.closest('[data-page]').dataset.page,b)));
    async function getHost(){if(!hostPromise)hostPromise=ManyspaceWasmHost.create({loadCoreText:async()=>window.ManyspaceCoreText || (workflow?await workflow.bundled_core.callPromising():await (await fetch('manyspace-core.js')).text()),onTelemetry:updateTelemetry}).then(h=>host=h);return hostPromise;}
    async function command(kind,value,extra={}){if(kind==='play' && value && busy)throw new Error('Wait for the circuit to finish before playing audio.');const h=await getHost();return h.call('command',{kind,value,...extra});}
    async function stop(){if(host && prepared)await command('play',0);playing=false;paintTransport();}
    function prepare(){const requestedPage=page;preparation=preparation.catch(()=>{}).then(async()=>{
      const resume=playing;await stop();telemetry=null;lastInstance=null;const h=await getHost();
      await h.call('prepare',{scene:M.copy(scene),task:requestedPage==='shape'?'shape':'play',preview});prepared=true;
      await command('bus',requestedPage==='shape'?shapeBus():effect?3:0);
      await command('trigger',requestedPage==='shape' || !effect || !kindChosen?0:{manual:0,auto:1,loop:2}[scene.performance.trigger]);
      if(resume){await command('play',1);playing=true;}paintTransport();
    });return preparation;}
    function shapeBus(){return pages.shape.querySelector('[name="s-bus"]:checked')?.value==='dry'?0:1;}
    async function navigate(next){await stop();if(recording)await record(false);page=next;root.querySelectorAll('[data-page]').forEach(e=>e.hidden=e.dataset.page!==next);root.querySelectorAll('.tabs [data-nav]').forEach(e=>{e.setAttribute('aria-current',e.dataset.nav===next?'page':'false');e.classList.toggle('on',e.dataset.nav===next);});status('');if(next==='play'){paintPlay();add(`page opened · manyfold of ${scene.rooms.length} rooms · seed ${scene.galaxy.generation.seed} · ${scene.galaxy.generation.backend}`);}if(next==='shape'){prepared=false;paintShape();}if(next==='play')prepared=false;if(next==='appendix')paintAppendix();pages[next].querySelector('h1')?.setAttribute('tabindex','-1');pages[next].querySelector('h1')?.focus();}
    root.querySelectorAll('[data-nav]').forEach(b=>b.addEventListener('click',act(()=>{const from=b.closest('[data-page]')?.dataset.page;if(gates[from])complete(from,b);return navigate(b.dataset.nav);})));
    function pair(p,r,commit){const a=el(p,r),b=el(p,r+'N');if(!a || !b)return;a.setAttribute('aria-label',b.getAttribute('aria-label') || r);bindPair(a,b,commit,act);}
    async function save(name,type,data){if(options.sendFile)return options.sendFile(name,type,data);const a=document.createElement('a'),url=URL.createObjectURL(new Blob([data],{type}));a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
    function load(value){const loaded=M.load(value);if(loaded.scene.rooms.length>64)throw new Error('Choose a manyfold with 16 or 64 rooms.');scene=loaded.scene;if((typeof value==='string'?JSON.parse(value):value).format!==M.S){scene.performance.trigger='auto';scene.performance.automatic=true;scene.performance.sensitivity=.58;}selected=scene.performance.room;arc=null;lastInstance=null;prepared=false;telemetry=null;paintCover();}
    // A · Generate
    pair('generate','depth');pair('generate','phase');pair('generate','shots');
    listen('generate','dice','click',e=>{el('generate','seed').value=Math.floor(Math.random()*0x100000000);complete('generate',e.target);});
    listen('generate','run','click',async e=>{
      if(busy || usedBackend)return;
      const count=+(pages.generate.querySelector('[name="g-size"]:checked')?.value || 0);if(!count)throw new Error('Choose 16 or 64 rooms.');
      if(!workflow || !backendHandle)throw new Error(options.generationHelp || 'Generate runs with the backend selected through Qollab.');
      const config=['seed','depth','phase','shots'].map(r=>+el('generate',r).value);
      const [seed,depth,phase,shots]=config;if(!Number.isInteger(seed)||seed<0||seed>0xffffffff||!Number.isInteger(depth)||depth<2||depth>8||!Number.isFinite(phase)||phase< -Math.PI||phase>Math.PI||!Number.isInteger(shots)||shots<1024||shots>65536||shots%1024)throw new Error('Check the seed, depth, phase and shots.');
      await stop();if(host)await host.call('suspend');busy=true;usedBackend=true;el('generate','run').disabled=true;pages.generate.querySelector('.nb').classList.add('busy');el('generate','params').textContent='running the circuit…';
      try{const q=unpack(await workflow.generate_on_backend.callPromising(backendHandle,count,...config));load(q);scene.name='zork';generationResult=scene.galaxy;paintGenerate();complete('generate',e.target);pages.generate.querySelector('[data-onion]').classList.add('off');gates.shape.done(0);pages.shape.querySelector('[data-onion]').classList.add('off');el('shape','name').value=scene.name;paintShape();}
      catch(e){el('generate','params').textContent=e.message;throw e;}
      finally{busy=false;pages.generate.querySelector('.nb').classList.remove('busy');}
    });
    listen('generate','dl','click',async e=>{await save('multi-verb.json','application/json',M.serialize(scene));complete('generate',e.target);});
    function paintGenerate(){const q=generationResult || scene.galaxy,n=q.rooms.length,tally=new Map();q.rooms.forEach(r=>tally.set(r.outcome,(tally.get(r.outcome)||0)+1));const grid=el('generate','outcomes');grid.style.gridTemplateColumns=`repeat(${Math.sqrt(n)},1fr)`;grid.setAttribute('aria-label',`Outcome tally, ${Math.sqrt(n)} by ${Math.sqrt(n)}`);grid.innerHTML=q.rooms.map(r=>`<i title="${r.outcome.toString(2).padStart(6,'0')}: ${tally.get(r.outcome)}" style="background:rgba(31,95,168,${.1+.8*tally.get(r.outcome)/Math.max(...tally.values())})"><span class="sr-only">${r.id}: ${r.outcome.toString(2).padStart(6,'0')}, ${tally.get(r.outcome)} rooms</span></i>`).join('');el('generate','tally').textContent=`${tally.size} unique outcomes of 64 · ${n} rooms`;const canvas=el('generate','heat');canvas.width=canvas.height=n;canvas.setAttribute('aria-label',`Transition probabilities, ${n} sources by ${n} destinations`);const c=canvas.getContext('2d'),max=Math.max(...q.transitions.flat());q.transitions.forEach((row,i)=>row.forEach((v,j)=>{c.fillStyle=`rgba(31,95,168,${v/max})`;c.fillRect(j,i,1,1);}));let values=pages.generate.querySelector('.odds-table');if(!values){values=document.createElement('details');values.className='odds-table';canvas.after(values);}values.innerHTML='<summary>Saved probability values</summary><div class="probability-scroll"><table><thead><tr><th>from ↓ to →</th>'+q.rooms.map((_,i)=>'<th>'+i+'</th>').join('')+'</tr></thead><tbody>'+q.transitions.map((row,i)=>'<tr><th>'+i+'</th>'+row.map(v=>'<td>'+(v*100).toFixed(3)+'%</td>').join('')+'</tr>').join('')+'</tbody></table></div>';el('generate','params').textContent=params();el('generate','stamp').style.visibility='visible';}
    // B · Shape
    listen('shape','useBuiltin','click',async e=>{await stop();load(reference);scene.name=el('shape','name').value || 'zork';complete('shape',e.target);pages.shape.querySelector('[data-onion]').classList.add('off');paintShape();});
    listen('shape','upload','change',async e=>{const f=e.target.files?.[0];if(!f)return;if(f.size>2*1024*1024)throw new Error('Choose a manyfold up to 2 MB.');await stop();load(JSON.parse(await f.text()));el('shape','name').value=scene.name;complete('shape',e.target);pages.shape.querySelector('[data-onion]').classList.add('off');paintShape();});
    listen('shape','name','change',e=>{scene.name=e.target.value;});
    el('shape','show').innerHTML=Object.entries(P).map(([k,p])=>`<option value="${k}">${p[0]}</option>`).join('')+'<option value="mode">treatment</option>';el('shape','show').value='speed';
    listen('shape','show','change',()=>paintShapeMap());
    function ranges(){const rows=Object.entries(P).map(([k,p])=>{const values=scene.mapping.ranges[k].map(v=>unitValue(k,v));return `<div class="rng"><span class="lbl">${p[6]>=0?`<i class="cat c${p[6]}"></i>`:''}${p[0]}<button type="button" class="q" data-q="${k}" aria-label="What ${p[0]} does" title="${esc(p[5])}" aria-expanded="false">?</button></span><div class="bar"><i style="left:${(values[0]-p[3][0])/(p[3][1]-p[3][0])*100}%;right:${100-(values[1]-p[3][0])/(p[3][1]-p[3][0])*100}%"></i></div><span class="ends">${values.map((v,i)=>`<input type="number" data-range="${k}" data-end="${i}" aria-label="${p[0]} ${i?'maximum':'minimum'}" min="${p[3][0]}" max="${p[3][1]}" step="${p[2]}" value="${displayNumber(v,p[7])}">${i?'':'–'}`).join('')}<span class="unit">${p[1]}</span></span><span class="hint rhint" data-hint="${k}" hidden>${p[5]}</span></div>`;});el('shape','ranges').innerHTML=rows.slice(0,6).join('');el('shape','ranges2').innerHTML=rows.slice(6).join('');}
    pages.shape.addEventListener('click',e=>{const b=e.target.closest('[data-q]');if(b){const h=pages.shape.querySelector(`[data-hint="${b.dataset.q}"]`);h.hidden=!h.hidden;b.setAttribute('aria-expanded',!h.hidden);complete('shape',b);}});
    pages.shape.addEventListener('change',act(async e=>{if(e.target.dataset.range){const k=e.target.dataset.range,end=+e.target.dataset.end,v=modelValue(k,+e.target.value),next=M.copy(scene);next.mapping.ranges[k][end]=v;if(next.mapping.ranges[k][0]>next.mapping.ranges[k][1])next.mapping.ranges[k][1-end]=v;scene=M.materialize(next);ranges();paintShape();if(prepared)await prepare();}if(e.target.dataset.edit){scene=M.edit(scene,scene.rooms[selected].id,e.target.dataset.edit,e.target.dataset.edit==='mode'?e.target.value:modelValue(e.target.dataset.edit,+e.target.value));paintShape();if(prepared)await prepare();}}));
    for(let i=0;i<3;i++)pair('shape','w'+i);
    listen('shape','rearrange','click',async e=>{const next=M.copy(scene),weights=[0,1,2].map(i=>+el('shape','w'+i).value),sum=weights.reduce((a,b)=>a+b,0);if(!sum)throw new Error('Choose at least one ordering weight.');const id=scene.rooms[selected].id;next.mapping.weights=weights.map(v=>v/sum);scene=M.arrange(next);selected=scene.order.indexOf(id);paintShape();complete('shape',e.target);if(prepared)await prepare();});
    listen('shape','restore','click',async()=>{scene=M.restore(scene,scene.rooms[selected].id);paintShape();if(prepared)await prepare();});
    listen('shape','phrase','change',async e=>{scene.performance.source=e.target.value;await prepare();});
    pages.shape.querySelectorAll('[name="s-prev"]').forEach(r=>r.addEventListener('change',act(async()=>{preview=r.value;el('shape','pathHint').hidden=preview!=='path';await prepare();})));
    pages.shape.querySelectorAll('[name="s-bus"]').forEach(r=>r.addEventListener('change',act(async()=>{if(!prepared)await prepare();await command('bus',shapeBus());})));
    listen('shape','play','click',async e=>{if(!prepared)await prepare();await command('play',1);playing=true;paintTransport();complete('shape',e.target);});listen('shape','stop','click',async e=>{await stop();complete('shape',e.target);});
    listen('shape','dl','click',async e=>{await save('multi-verb.json','application/json',M.serialize(scene));complete('shape',e.target);});
    function roomCard(r){const d=words(r,r.derived);return `<b>${esc(d.name)}</b><span>${esc(d.facts)}</span><small>${esc(r.id)}</small>`;}
    function paintShapeMap(){drawMap(pages.shape,scene,selected,el('shape','show').value);}
    function paintShape(){const custom=scene.performance.source==='custom';pages.shape.querySelector('[name="s-prev"][value="path"]').disabled=custom;if(custom)preview='room';if(custom && !el('shape','phrase').querySelector('[value=custom]'))el('shape','phrase').insertAdjacentHTML('beforeend','<option value="custom">uploaded audio</option>');el('shape','phrase').value=scene.performance.source;if(custom){pages.shape.querySelector('[name="s-prev"][value="room"]').checked=true;el('shape','pathHint').hidden=true;}paintShapeMap();ranges();const r=scene.rooms[selected];el('shape','roomTitle').textContent='Room '+selected;el('shape','roomTag').textContent=r.edited.length?'edited · '+r.edited.length:'from the circuit';el('shape','roomWords').innerHTML=roomCard(r);el('shape','adj').textContent=`neighbour distance ${scene.evidence.after.mean.toFixed(2)} (lower = smoother)`;el('shape','insp').innerHTML=Object.entries(P).map(([k,p])=>`<label class="${r.edited.includes(k)?'edited':''}">${p[0]}<input type="number" data-edit="${k}" min="${p[3][0]}" max="${p[3][1]}" step="${p[2]}" value="${displayNumber(unitValue(k,r[k]),p[7])}"></label>`).join('')+`<label>treatment<select data-edit="mode"><option value="decay" ${r.mode==='decay'?'selected':''}>forward</option><option value="swell" ${r.mode==='swell'?'selected':''}>reverse</option></select></label>`;el('shape','restore').disabled=!r.edited.length;paintChart();}
    function paintChart(){const a=scene.transitions[selected],b=scene.dephased[selected],max=Math.max(...a,...b),width=420/a.length;el('shape','chart').innerHTML=a.map((v,i)=>`<rect x="${i*width}" y="${120-v/max*110}" width="${width/2}" height="${v/max*110}" fill="#1f5fa8"/><rect x="${i*width+width/2}" y="${120-b[i]/max*110}" width="${width/2}" height="${b[i]/max*110}" fill="#6e685e"/>`).join('');let table=pages.shape.querySelector('.odds-table');if(!table){table=document.createElement('details');table.className='odds-table';el('shape','chart').after(table);}table.innerHTML='<summary>Saved probability values</summary><table><thead><tr><th>room</th><th>quantum odds</th><th>plain chance</th></tr></thead><tbody>'+a.map((v,i)=>`<tr><th>${i}</th><td>${(v*100).toFixed(3)}%</td><td>${(b[i]*100).toFixed(3)}%</td></tr>`).join('')+'</tbody></table>';}
    // C · Play
    for(const p of ['play','shape']){const select=el(p,p==='play'?'sample':'phrase');select.innerHTML=[['plucks','plucks · 96 BPM'],['drums','drums · 110 BPM'],['chords','chords · 72 BPM']].map(([v,t])=>`<option value="${v}">${t}</option>`).join('');}
    listen('play','sample','change',async e=>{scene.performance.source=e.target.value;el('play','fileName').textContent='';add('input: '+e.target.selectedOptions[0].text);await prepare();});
    listen('play','upload','change',async e=>{const f=e.target.files?.[0];if(!f)return;if(f.size>32*1024*1024)throw new Error('Choose an audio file up to 32 MB.');await stop();const h=await getHost();await h.call('loadAudio',{bytes:await f.arrayBuffer(),name:f.name});scene.performance.source='custom';el('play','fileName').textContent=f.name;add(`input: file ${f.name} (${Math.round(f.size/1024)} kB)`);await prepare();complete('play',e.target);});
    for(const mode of ['once','loop'])listen('play',mode,'click',async e=>{scene.performance.loop=mode==='loop';if(!prepared)await prepare();await command('loop',scene.performance.loop?1:0);await command('play',1);playing=true;paintTransport();add(mode==='loop'?'play, looping':'play once');complete('play',e.target);});
    listen('play','stop','click',async e=>{await stop();add('stop');complete('play',e.target);});
    scene.performance.trigger='auto';scene.performance.automatic=true;scene.performance.sensitivity=.58;
    function triggerBoxes(){el('play','manualBox').hidden=false;el('play','autoBox').hidden=scene.performance.trigger!=='auto';el('play','eachBox').hidden=scene.performance.trigger!=='loop';el('play','trigger').disabled=!kindChosen;}
    for(const [r,mode] of [['manual','manual'],['auto','auto'],['each','loop']])listen('play',r,'change',async()=>{scene.performance.trigger=mode;scene.performance.automatic=mode==='auto';triggerBoxes();add('trigger: '+(mode==='loop'?'each loop':mode==='auto'?'auto at '+Math.round(scene.performance.sensitivity*100)+' %':'manual'));if(prepared)await command('trigger',effect && kindChosen?{manual:0,auto:1,loop:2}[mode]:0);});
    pair('play','thr',async v=>{scene.performance.sensitivity=v/100;add('trigger level '+v+' %');if(prepared)await knobs();});
    pair('play','mix',async v=>{scene.performance.mix=v/100;add('immersion '+v+' %');if(prepared)await knobs();});pair('play','decay',async v=>{scene.performance.decay=v;add('linger '+v+' s');if(prepared)await knobs();});
    async function knobs(){return command('knobs',0,{mix:scene.performance.mix,decay:scene.performance.decay,sensitivity:scene.performance.sensitivity});}
    for(const [r,mode] of [['step','next'],['leap','leap']])listen('play',r,'change',async()=>{scene.performance.traversal=mode;arc=null;kindChosen=true;paintPlay();add('room change: '+(mode==='next'?'next room over':'quantum leap'));if(prepared)await command('traversal',mode==='next'?0:1);});
    listen('play','trigger','click',async()=>{if(!effect){add('manual trigger ignored, effect is off');return;}if(!prepared)await prepare();pendingReason='manual';await command(scene.performance.traversal);});
    listen('play','effect','change',async e=>{effect=e.target.checked;if(effect){lifted=true;complete('play',e.target);}pages.play.querySelector('[data-onion]').classList.toggle('off',lifted);if(!prepared)await prepare();await command('bus',effect?3:0);await command('trigger',effect && kindChosen?{manual:0,auto:1,loop:2}[scene.performance.trigger]:0);add(effect?'effect on':'effect off: original sound only');});
    for(const p of ['play','shape'])pages[p].addEventListener('click',act(async e=>{const b=e.target.closest('[data-room]');if(!b)return;selected=+b.dataset.room;scene.performance.room=selected;if(p==='shape')paintShape();else{pendingReason='by hand';paintPlay();}if(prepared)await command('room',selected);}));
    async function record(on){if(!prepared)await prepare();await host.call('record',{value:on});recording=on;el('play','rec').textContent=on?'■ stop take '+(takeNumber+1):'● record a take';el('play','rec').setAttribute('aria-pressed',on);el('play','recDl').disabled=on || !host.snapshot().take_seconds;if(!on)takeNumber++;add(on?'REC start, take '+(takeNumber+1):`REC stop, take ${takeNumber} kept · ${host.snapshot().take_seconds.toFixed(1)} s`);}
    listen('play','rec','click',async e=>{await record(!recording);complete('play',e.target);});listen('play','recDl','click',async()=>{const bytes=await host.call('take');await save(`take-${String(takeNumber).padStart(2,'0')}.wav`,'audio/wav',bytes);add('download take-'+String(takeNumber).padStart(2,'0')+'.wav');});el('play','recDl').disabled=true;
    listen('play','logDl','click',()=>{add('download experiment-log.txt');complete('play',el('play','logDl'));return save('experiment-log.txt','text/plain',el('play','log').value+'\n');});el('play','log').readOnly=false;
    function paintTransport(){for(const p of ['play','shape']){for(const r of ['once','loop','stop','play']){const b=el(p,r);if(b)b.setAttribute('aria-pressed',r==='stop'?!playing:r==='play'?playing:playing && (r==='loop')===scene.performance.loop);}}}
    function paintPlay(){const p=scene.performance;for(const [r,v] of [['mix',p.mix*100],['decay',p.decay],['thr',p.sensitivity*100]]){if(document.activeElement!==el('play',r) && document.activeElement!==el('play',r+'N'))el('play',r).value=el('play',r+'N').value=displayNumber(v,stepPrecision(el('play',r+'N').step));}for(const [r,v] of [['manual','manual'],['auto','auto'],['each','loop']])el('play',r).checked=p.trigger===v;if(p.source==='custom' && !el('play','sample').querySelector('[value=custom]'))el('play','sample').insertAdjacentHTML('beforeend','<option value="custom">uploaded audio</option>');el('play','sample').value=p.source;const now=telemetry && prepared ? telemetry.room : scene.performance.room;drawMap(pages.play,scene,now,'speed',telemetry?.voices || [],arc);const voices=telemetry?.voices || [];el('play','now').innerHTML=`<div class="nowcard"><span>now</span>${roomCard(scene.rooms[now])}</div><div class="ringing"><span>still ringing</span>${voices.filter(v=>v.active && v.age>0).map(v=>`<i><span>${esc(scene.rooms[v.room]?.id || v.room)}</span><span>${(20*Math.log10(Math.max(1e-4,v.gain))).toFixed(0)} dB</span></i>`).join('') || '<i>nothing yet</i>'}</div>`;triggerBoxes();}
    function updateTelemetry(t){root.dataset.overruns=t.overruns;root.dataset.maxMs=t.max_ms;root.dataset.passes=t.passes;const old=telemetry;telemetry=t;playing=t.playing;scene.performance.room=t.room;if(page==='play'){
      for(const change of t.events || []) {if(scene.performance.traversal==='leap' && change.reason!=='by hand')arc=[change.from,change.to];else arc=null;add(`${change.reason} ${scene.performance.traversal==='leap' && change.reason!=='by hand'?'LEAP':'step'} ${change.from} → ${change.to} · ${words(scene.rooms[change.to]).name}`);}
      if(t.dropped_events>(old?.dropped_events || 0))add('some room changes exceeded the log buffer');
      const meter=el('play','vuFill').parentElement;meter.setAttribute('aria-valuenow',Math.max(0,Math.min(1,t.envelope)));meter.setAttribute('aria-valuetext',`${Math.round(t.envelope*100)} percent dry level; trigger threshold ${Math.round(t.threshold*100)} percent`);el('play','passes').textContent=`${t.passes} ${t.passes===1?'loop':'loops'} so far`;el('play','vuFill').style.width=Math.min(100,t.envelope*100)+'%';el('play','vuMark').style.left=Math.min(100,t.threshold*100)+'%';paintPlay();
      if(recording && !t.recording){recording=false;takeNumber++;el('play','rec').textContent='● record a take';el('play','rec').setAttribute('aria-pressed','false');el('play','recDl').disabled=false;add(`REC stop, take ${takeNumber} kept · ${t.take_seconds.toFixed(1)} s`);}
    }lastInstance=t.instances;paintTransport();}
    function paintCover(){const n=scene.rooms.length,bits=Math.log2(n);el('cover','coverWalk').textContent=`A ${bits}-qubit walk on a hypercube: its ${n} corners become the rooms, and where the walk lands sets the odds of each leap. Six more qubits give every room its sound.`;drawMap(pages.cover,scene,0,'speed',[],null,true);}
    function params(){const g=scene.galaxy.generation;return `rooms ${scene.rooms.length} · depth ${g.depth} · phase ${displayNumber(g.phase,2)} · seed ${g.seed} · shots ${g.shots} · backend: ${g.backend}`;}
    function paintAppendix(){const g=scene.galaxy.generation,dl=pages.appendix.querySelector('dl');dl.innerHTML=Object.entries({name:scene.name,rooms:scene.rooms.length,depth:g.depth,phase:displayNumber(g.phase,2),seed:g.seed,shots:g.shots,backend:g.backend,'unique outcomes':new Set(scene.galaxy.rooms.map(r=>r.outcome)).size+' of 64'}).map(([k,v])=>`<dt>${esc(k)}</dt><dd>${esc(v)}</dd>`).join('');const pre=pages.appendix.querySelector('pre');pre.innerHTML=Object.entries(scene.galaxy.circuits).map(([k,v])=>esc(k)+'\n'+esc(v)).join('\n\n');}
    const meter=el('play','vuFill').parentElement;meter.removeAttribute('aria-hidden');meter.setAttribute('role','meter');meter.setAttribute('aria-label','dry level');meter.setAttribute('aria-valuemin','0');meter.setAttribute('aria-valuemax','1');meter.setAttribute('aria-valuenow','0');
    triggerBoxes();paintShape();paintPlay();paintAppendix();paintCover();
    let miniRoom=0;const miniTimer=setInterval(()=>{if(page!=='cover' || window.matchMedia?.('(prefers-reduced-motion: reduce)').matches)return;let draw=Math.random(),dest=scene.transitions[miniRoom].length-1;for(let i=0;i<scene.rooms.length;i++){draw-=scene.transitions[miniRoom][i];if(draw<=0){dest=i;break;}}miniRoom=dest;drawMap(pages.cover,scene,miniRoom,'speed',[],null,true);},1800);
    return {navigate,get scene(){return scene;},get host(){return host;},gates,async close(){clearInterval(miniTimer);await host?.close();}};
  }
  return {words,gate,drawMap,bindPair,displayNumber,mount};
})();
if(typeof module!=='undefined' && module.exports)module.exports=ManyspaceNotebook;
