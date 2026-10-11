'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),crypto=require('node:crypto'),vm=require('node:vm');
const context=vm.createContext({});vm.runInContext(fs.readFileSync('web/notebook-art.js','utf8'),context);
const choices=context.ManyspaceNotebookArt.choices;
const selected=JSON.parse(fs.readFileSync('design/art-proposals/selected.json')).proposals;
assert.equal(choices.length,selected.length);
for(const p of selected){const c=choices.find(c=>c[0]===p.id);assert.ok(c,p.id);assert.deepEqual(Array.from(c).slice(1,3),[p.asset,p.page]);assert.equal(c[5],p.width);assert.equal(c[7],p.rotation);assert.equal(c[8],p.color);}
const css=fs.readFileSync('web/notebook-art.css','utf8'),manifest=JSON.parse(fs.readFileSync('design/art-processed/manifest.json'));
for(const [name,m] of Object.entries(manifest)){
 // The minimal distribution carries derivatives; full-resolution originals stay in the authoring workspace.
 if(fs.existsSync('design/art/'+m.source)){
  const original=fs.readFileSync('design/art/'+m.source);
  assert.equal(crypto.createHash('sha256').update(original).digest('hex'),m.source_sha256,'original preserved');
 }
 const bytes=fs.readFileSync('design/art-processed/'+name+'.webp');
 assert.equal(bytes.length,m.bytes);assert.equal(bytes.subarray(0,4).toString(),'RIFF');assert.equal(bytes.subarray(8,12).toString(),'WEBP');
 assert.equal(css.split(bytes.toString('base64')).length-1,1,'each unique image embedded once');
}
assert.doesNotMatch(css,/url\(["']?https?:/);
assert.ok(css.length<500000,'soft inline art budget');
assert.ok(['web/notebook-fonts.css','web/notebook.css','web/notebook-art.css'].reduce((n,p)=>n+fs.statSync(p).size,0)<2000000,'complete font/layout/art stylesheet fits');
assert.ok(fs.readFileSync('web/notebook.js','utf8').includes('ManyspaceNotebookArt.apply(pages)'));
console.log('notebook art: selected placements, source preservation, unique embedded assets and bundle budgets pass');
