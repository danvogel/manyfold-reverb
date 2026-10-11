(async()=>{
  const options=window.ManyspaceNotebookTestOptions || {};
  if(!options.workflow) {
    options.generationHelp='Local Generate needs the simulator server. Stop the static server and run: python3 tools/notebook_server.py';
    try {
      const response=await fetch('/api/backend');
      if(response.ok && (await response.json()).name==='local Qiskit BasicSimulator') {
        options.backend={name:'local Qiskit BasicSimulator'};
        options.workflow={generate_on_backend:{async callPromising(backend,count,seed,depth,phase,shots) {
          const response=await fetch('/api/generate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({count,seed,depth,phase,shots})});
          const result=await response.json();if(!response.ok)throw new Error(result.error || 'Local generation failed');return result;
        }}};
      }
    } catch(_) { /* The static preview still supports Shape and Play. */ }
  }
  await ManyspaceNotebook.mount(options);
})().catch(e=>document.getElementById('app').textContent=e.message);
