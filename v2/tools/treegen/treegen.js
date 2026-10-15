const { chromium } = require('/home/claude/ebarge/node_modules/playwright');
const http=require('http'),fs=require('fs'),path=require('path');
const ROOT='/home/claude/albert-canal-vr', NM='/home/claude/v2tools/node_modules';
const srv=http.createServer((q,r)=>{ let u=decodeURIComponent(q.url.split('?')[0]); if(u.endsWith('/')) u+='index.html'; fs.readFile(path.join(ROOT,u),(e,d)=>{ if(e){r.writeHead(404);r.end();return;} r.writeHead(200,{'Content-Type':u.endsWith('.html')?'text/html':'application/octet-stream'}); r.end(d); }); }).listen(8768);
(async()=>{
 const b=await chromium.launch({executablePath:'/opt/pw-browsers/chromium',args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 const p=await b.newPage(); p.on('pageerror',e=>console.log('ERR',e.message)); p.on('console',m=>{ if(m.type()==='error') console.log('CON',m.text().slice(0,200)); });
 await p.route('https://cdn.jsdelivr.net/npm/**',r=>{ const u=r.request().url().replace('https://cdn.jsdelivr.net/npm/',''); const m=u.match(/^(@[^/]+\/[^@/]+|[^@/]+)@[^/]+\/(.*)$/); const f=path.join(NM,m[1],m[2]); r.fulfill({path:f,contentType:'text/javascript'}); });
 await p.goto('http://localhost:8768/_treegen/index.html'); await p.waitForFunction(()=>window.ready,null,{timeout:60000});
 const res=await p.evaluate(()=>window.run());
 fs.mkdirSync('/home/claude/v2tools/trees',{recursive:true});
 for(const k in res.out) fs.writeFileSync('/home/claude/v2tools/trees/'+k+'.glb',Buffer.from(res.out[k],'base64'));
 fs.writeFileSync('/home/claude/v2tools/trees/stats.json',JSON.stringify(res.stats,null,1));
 console.log(JSON.stringify(res.stats));
 await b.close(); srv.close();
})();
