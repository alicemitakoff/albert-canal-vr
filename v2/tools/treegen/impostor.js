const { chromium } = require('/home/claude/ebarge/node_modules/playwright');
const http=require('http'),fs=require('fs'),path=require('path');
const ROOT='/home/claude/albert-canal-vr', NM='/home/claude/v2tools/node_modules';
const MIME={'.html':'text/html','.jpg':'image/jpeg','.png':'image/png','.glb':'model/gltf-binary'};
const srv=http.createServer((q,r)=>{ let u=decodeURIComponent(q.url.split('?')[0]); fs.readFile(path.join(ROOT,u),(e,d)=>{ if(e){r.writeHead(404);r.end();return;} r.writeHead(200,{'Content-Type':MIME[path.extname(u)]||'application/octet-stream'}); r.end(d); }); }).listen(8769);
(async()=>{
 const st=JSON.parse(fs.readFileSync('/home/claude/v2tools/trees/stats.json'));
 const ids=['oak_m','oak_l','pine_m','pine_l','birch','poplar','ash_m','aspen_s'];
 const SP=ids.map(id=>{ const s=st[id+'_0']; return {id,bark:s.barkType,leaf:s.leafType,barkTint:s.barkTint,leafTint:s.leafTint,tx:s.texScale.x,ty:s.texScale.y,alpha:s.alphaTest,w:+s.width}; });
 const b=await chromium.launch({executablePath:'/opt/pw-browsers/chromium',args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 const p=await b.newPage(); p.on('pageerror',e=>console.log('ERR',e.message)); p.on('console',m=>{ if(m.type()==='error') console.log('CON',m.text().slice(0,200)); });
 await p.route('https://cdn.jsdelivr.net/npm/**',r=>{ const u=r.request().url().replace('https://cdn.jsdelivr.net/npm/',''); const m=u.match(/^(@[^/]+\/[^@/]+|[^@/]+)@[^/]+\/(.*)$/); r.fulfill({path:path.join(NM,m[1],m[2]),contentType:'text/javascript'}); });
 await p.goto('http://localhost:8769/_treegen/impostor.html'); await p.waitForFunction(()=>window.ready,null,{timeout:60000});
 const res=await p.evaluate(SP=>window.run(SP),SP);
 fs.writeFileSync('/home/claude/v2tools/trees/impostors.png',Buffer.from(res.png.split(',')[1],'base64'));
 SP.forEach((s,i)=>s.tileW=res.tileW[i]); fs.writeFileSync('/home/claude/v2tools/trees/species.json',JSON.stringify(SP));
 console.log(JSON.stringify(SP)); await b.close(); srv.close();
})();
