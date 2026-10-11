const fs=require('node:fs'),M=require('../web/model.js');
fs.mkdirSync('build/workflow',{recursive:true});
fs.writeFileSync('build/workflow/scene16.json',M.serialize(M.load(JSON.parse(fs.readFileSync('presets/reference16.json','utf8'))).scene));
