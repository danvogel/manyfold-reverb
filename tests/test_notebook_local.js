'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const code=fs.readFileSync('web/notebook-local.js','utf8');
(async()=>{
 let options,calls=[];
 const context={window:{},document:{getElementById:()=>({})},ManyspaceNotebook:{async mount(o){options=o;}},async fetch(url,config){calls.push([url,config]);return {ok:true,async json(){return url==='/api/backend'?{name:'local Qiskit BasicSimulator'}:{format:'manyspace.quantum-galaxy'};}};}};
 await vm.runInNewContext(code,context);
 assert.equal(options.backend.name,'local Qiskit BasicSimulator');
 const q=await options.workflow.generate_on_backend.callPromising(options.backend,64,11,4,.2,8192);
 assert.equal(q.format,'manyspace.quantum-galaxy');assert.equal(calls[1][0],'/api/generate');assert.equal(calls[1][1].method,'POST');assert.deepEqual(JSON.parse(calls[1][1].body),{count:64,seed:11,depth:4,phase:.2,shots:8192});
 context.window={};context.fetch=async()=>({ok:false});await vm.runInNewContext(code,context);assert.equal(options.workflow,undefined);assert.match(options.generationHelp,/python3 tools\/notebook_server.py/);
 context.window={ManyspaceNotebookTestOptions:{workflow:{fixture:true}}};context.fetch=async()=>{throw new Error('fixture must not probe server');};await vm.runInNewContext(code,context);assert.equal(options.workflow.fixture,true);
 console.log('local notebook: simulator discovery, requested job settings, static fallback and fixture isolation pass');
})().catch(e=>{console.error(e);process.exitCode=1});
