/* analogBEAST v1.0 SEQ web editor. GPL-3.0-only.
 * Uses the same SysEx protocol/JSON structure as tools/analogbeast_transfer.py.
 * Independent of the Felucca editor's existing MIDI handlers and data models.
 */
(function (root) {
'use strict';
const MAGIC=[0x7d,0x46,0x4c,96], BANKS='ABCDEFGH', FIELDS=['note','flags','velocity','gate','swing'];
const DEFAULT_STEP=()=>({note:0,flags:0,velocity:96,gate:64,swing:0});
const clone=v=>JSON.parse(JSON.stringify(v));
function integer(v,min,max,name) {if (!Number.isInteger(v)||v<min||v>max) throw Error(`${name}: ${min}..${max} erforderlich (${v})`);return v;}
function validateRow(row){
 if(!Array.isArray(row)||row.length!==4)throw Error('SONG-Zeile muss 4 Zahlen enthalten');
 const [b,p,steps,val]=row;
 integer(b,0,12,'Befehl');integer(p,0,127,'Pattern');integer(steps,1,64,'Steps');integer(val,0,255,'Wert');
 if (b<=7) { if(p>=16||val!==0||(b>=4&&steps>16))throw Error('Ungültige Patternzeile'); }
 else if(b===8){if(p!==16||val!==0)throw Error('END: falsche Daten');}
 else if(p!==0)throw Error('Befehl: Pattern muss 0 sein');
 else if(b===9 && (val<40||val>240))throw Error('TEMPO 40..240');
 else if(b===10 && val>=32)throw Error('PRESET 0..31');
 else if(b===11 && val!==0)throw Error('REST: Wert muss 0 sein');
 else if(b===12 && val>1)throw Error('ARP: Wert 0 oder 1');
 return row;
}
function checkRecord(type,r){
 if(!r||typeof r!=='object'||Array.isArray(r))throw Error('Datensatz ungültig');
 if(type==='pattern'){
  if(typeof r.bank!=='string'||r.bank.length!==1||!BANKS.includes(r.bank))throw Error('Bank A..H erforderlich');
  integer(r.pattern,1,16,'Pattern');const n=r.bank<='D'?64:16;
  integer(r.length,1,n,'Patternlänge');integer(r.resolution,0,n===64?9:5,'Auflösung');
  if(!Array.isArray(r.steps)||r.steps.length!==n)throw Error(`Bank ${r.bank}: genau ${n} Steps nötig`);
  for(const s of r.steps){if(!s||Object.keys(s).sort().join(',')!==FIELDS.slice().sort().join(','))throw Error('Jeder Step braucht note,flags,velocity,gate,swing');
   for(const [k,lo,hi] of [['note',0,127],['flags',0,31],['velocity',1,127],['gate',1,127],['swing',0,100]])integer(s[k],lo,hi,k);
  }
 }else if(type==='song'){
  integer(r.song,1,32,'Song');integer(r.count,1,64,'Zeilenanzahl');integer(r.loop,0,1,'Loop');
  if(!Array.isArray(r.rows)||r.rows.length!==64)throw Error('SONG benötigt 64 Zeilen');r.rows.forEach(validateRow);
 }else if(type==='album'){
  integer(r.album,1,4,'Album');integer(r.count,0,32,'Anzahl');integer(r.loop,0,1,'Loop');
  if(!Array.isArray(r.song_ids)||r.song_ids.length!==32)throw Error('ALBUM benötigt 32 Slots');
  r.song_ids.forEach((id,i)=>{if(i<r.count)integer(id,1,32,'Song-ID');else if(id!==null)throw Error('ALBUM: Nach END müssen Slots leer sein');});
 }else throw Error('Unbekannter Typ');return r;
}
function parseDocument(doc){
 if(!doc||doc.format!=='analogBEAST-v1.0')throw Error('JSON-Format muss analogBEAST-v1.0 sein');
 const entries=[],seen=new Set();
 for(const [plural,type] of [['patterns','pattern'],['songs','song'],['albums','album']]){
  const rs=doc[plural]||[];if(!Array.isArray(rs))throw Error(plural+' ist keine Liste');
  for(const r of rs){checkRecord(type,r);const key=keyOf(type,r);if(seen.has(key))throw Error('Doppelte Adresse '+key);seen.add(key);entries.push({type,record:clone(r)});}
 }if(!entries.length)throw Error('Keine Datensätze im Import');return entries;
}
function keyOf(type,r){return type==='pattern'?`pattern:${r.bank}:${r.pattern}`:type==='song'?`song:${r.song}`:`album:${r.album}`;}
function docOf(items){const d={format:'analogBEAST-v1.0',patterns:[],songs:[],albums:[]};for(const i of items)d[i.type==='pattern'?'patterns':i.type==='song'?'songs':'albums'].push(clone(i.record));return d;}
function eq(a,b){return JSON.stringify(a)===JSON.stringify(b);}
class Protocol {
 constructor(output,input){this.output=output;this.input=input;this.token=0;}
 request(op,data=[]){
  if(!data.every(x=>Number.isInteger(x)&&x>=0&&x<=127))return Promise.reject(Error('SysEx nur 7-Bit Daten'));
  const token=this.token=(this.token+1)&127, output=this.output, input=this.input;
  return new Promise((resolve,reject)=>{
   let timer;const finish=(err,body)=>{clearTimeout(timer);input.removeEventListener('midimessage',onMsg);err?reject(err):resolve(body);};
   const onMsg=(e)=>{const a=[...e.data];if(a[0]!==240||a[a.length-1]!==247||a.length<9||!eq(a.slice(1,5),MAGIC)||a[5]!==op||a[6]!==token)return;
    if(a[7])finish(Error(`MIDI op ${op}: Gerätestatus ${a[7]} (STOP/REC prüfen)`));else finish(null,a.slice(8,-1));};
   input.addEventListener('midimessage',onMsg);
   timer=setTimeout(()=>finish(Error(`MIDI timeout bei op ${op}; Verbindung/STOP prüfen`)),6000);
   try{output.send([240,...MAGIC,op,token,...data,247]);}catch(e){finish(e);}
  });
 }
 async read(type,key){
  if(type==='song'){
   const id=key.song-1,m=await this.request(28,[id]);if(m.length!==3||m[0]!==id)throw Error('Ungültige SONG-Metadaten');
   const rows=[];
   for(let start=0;start<64;start+=16){const r=await this.request(24,[id,start,16]);if(r.length!==83||!eq(r.slice(0,3),[id,start,16]))throw Error('SONG Lesefehler');
    for(let i=0;i<16;i++){const v=r.slice(3+i*5,8+i*5);rows.push([v[0],v[1],v[2],v[3]|(v[4]<<7)]);}}
   return checkRecord('song',{song:key.song,count:m[1],loop:m[2],rows});
  }
  if(type==='album'){
   const id=key.album-1,r=await this.request(26,[id]);if(r.length!==35||r[0]!==id)throw Error('ALBUM Lesefehler');
   return checkRecord('album',{album:key.album,count:r[1],loop:r[2],song_ids:r.slice(3).map(v=>v===127?null:v+1)});
  }
  const b=BANKS.indexOf(key.bank),p=key.pattern-1;let length,res,data=[];
  if(b>=4){const r=await this.request(22,[b,p]);if(r.length!==84||!eq(r.slice(0,2),[b,p]))throw Error('E–H Lesefehler');[length,res]=r.slice(2,4);data=r.slice(4);}
  else {const m=await this.request(17,[b,p]);if(m.length!==4||!eq(m.slice(0,2),[b,p]))throw Error('A–D Metadatenfehler');[length,res]=m.slice(2);
   for(let start=0;start<64;start+=16){const r=await this.request(15,[b,p,start,16]);if(r.length!==84||!eq(r.slice(0,4),[b,p,start,16]))throw Error('A–D Step-Lesefehler');data.push(...r.slice(4));}}
  return checkRecord('pattern',{bank:key.bank,pattern:key.pattern,length,resolution:res,steps:Array.from({length:data.length/5},(_,i)=>Object.fromEntries(FIELDS.map((f,j)=>[f,data[i*5+j]])))});
 }
 async write(type,r){checkRecord(type,r);
  if(type==='song'){
   const id=r.song-1;
   for(let start=0;start<64;start+=16){const data=[id,start,16];for(const [b,p,steps,v] of r.rows.slice(start,start+16))data.push(b,p,steps,v&127,v>>7);
    if(!eq(await this.request(23,data),[id,start,16]))throw Error('SONG Write-ACK falsch');}
   if(!eq(await this.request(27,[id,r.count,r.loop]),[id,r.count,r.loop]))throw Error('SONG Meta-ACK falsch');return;
  }
  if(type==='album'){
   const id=r.album-1,data=[id,r.count,r.loop,...r.song_ids.map(v=>v===null?127:v-1)];if(!eq(await this.request(25,data),data.slice(0,3)))throw Error('ALBUM Write-ACK falsch');return;
  }
  const b=BANKS.indexOf(r.bank),p=r.pattern-1,flat=r.steps.flatMap(s=>FIELDS.map(f=>s[f]));
  if(b>=4){if(!eq(await this.request(21,[b,p,r.length,r.resolution,...flat]),[b,p]))throw Error('E–H Write-ACK falsch');}
  else {
   for(let start=0;start<64;start+=16){const data=[b,p,start,16,...flat.slice(start*5,(start+16)*5)];if(!eq(await this.request(14,data),[b,p,start,16]))throw Error('A–D Step-ACK falsch');}
   if(!eq(await this.request(16,[b,p,r.length,r.resolution]),[b,p,r.length,r.resolution]))throw Error('A–D Meta-ACK falsch');
  }
 }
 async waitSaved(items,progress=()=>{}){
  const v4=items.some(i=>i.type==='pattern'&&i.record.bank<='D'),v5=items.some(i=>i.type!=='pattern'||i.record.bank>='E');
  const deadline=Date.now()+45000;
  while(Date.now()<deadline){
   let dirty=false;
   if(v4){const s=await this.request(6);if(s.length!==2||s[0]>1||s[1]>1)throw Error('V4 Speicherstatus ungültig');dirty=dirty||Boolean(s[0]);}
   if(v5){const s=await this.request(30);if(s.length!==5)throw Error('V5 Speicherstatus ungültig');if(s[1]||!s[2])throw Error('V5 NOR Autosave fehlgeschlagen: '+s.join(','));dirty=dirty||Boolean(s[0])||!s[3];}
   if(!dirty)return;
   progress('Warte auf Gerätespeicherung …');await new Promise(res=>setTimeout(res,500));
  }
  throw Error('Autosave nicht bestätigt – Gerät nicht ausschalten; Backup prüfen');
 }
}
root.ABSEQ={MAGIC,BANKS,FIELDS,DEFAULT_STEP,validateRow,checkRecord,parseDocument,docOf,keyOf,Protocol};
if(typeof module!=='undefined'&&module.exports)module.exports=root.ABSEQ;
if(typeof document==='undefined')return;
function init(){
 if(document.getElementById('abseq-open'))return;
 const css=document.createElement('style');css.textContent=`
  #abseq-open{position:fixed;right:12px;top:12px;z-index:2147483000;border:2px solid #ff982c;background:#25201b;color:#ffd49d;font-weight:700;border-radius:8px;padding:8px 14px;box-shadow:0 2px 15px #0006;cursor:pointer}
  #abseq-panel{position:fixed;z-index:2147482999;inset:0;overflow:auto;background:#14171b;color:#eaeef2;font:14px/1.45 system-ui,Arial,sans-serif;display:none}
  #abseq-panel.abseq-visible{display:block} #abseq-panel *{box-sizing:border-box}
  #abseq-panel .abseq-wrap{max-width:1240px;margin:auto;padding:18px 20px 55px}
  #abseq-panel h1{color:#ffad56;font-size:22px;margin:0} #abseq-panel h2{font-size:17px;color:#ffad56;margin:10px 0}
  #abseq-panel .abseq-top{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:12px;margin-bottom:15px}
  #abseq-panel .abseq-bar{display:flex;flex-wrap:wrap;gap:7px;align-items:center;background:#252b32;padding:12px;border-radius:8px;margin:10px 0}
  #abseq-panel button,#abseq-panel select,#abseq-panel input{font:inherit;color:#eef2f7;background:#323942;border:1px solid #66717d;border-radius:5px;padding:5px 7px}
  #abseq-panel button{cursor:pointer} #abseq-panel button:hover{border-color:#ffad56}
  #abseq-panel button.abseq-main{background:#b85c09;border-color:#eb8d3b;color:white;font-weight:700}
  #abseq-panel .abseq-tab[aria-pressed=true]{background:#b85c09;border-color:#ffad56}
  #abseq-panel input[type=number]{width:76px} #abseq-panel .abseq-sm{width:60px!important}
  #abseq-panel .abseq-muted{color:#b0bac6} #abseq-panel .abseq-warning{color:#ffd49d}
  #abseq-panel .abseq-tablewrap{overflow:auto;max-height:62vh;border:1px solid #4d5966;border-radius:7px}
  #abseq-panel table{width:100%;border-collapse:collapse;min-width:690px}#abseq-panel th,#abseq-panel td{padding:5px 7px;border-bottom:1px solid #323942;text-align:left}
  #abseq-panel th{background:#29323b;position:sticky;top:0;z-index:1}#abseq-panel tr:nth-child(even){background:#1c2228}
  #abseq-panel tr.abseq-inactive{opacity:.45}#abseq-panel .abseq-status{white-space:pre-wrap;padding:10px;border-radius:7px;background:#262d35;min-height:43px}
  #abseq-panel .abseq-right{margin-left:auto} #abseq-panel [hidden]{display:none!important}
 `;document.head.appendChild(css);
 const open=document.createElement('button');open.id='abseq-open';open.type='button';open.textContent='analogBEAST · SEQ';document.body.appendChild(open);
 const panel=document.createElement('section');panel.id='abseq-panel';panel.setAttribute('aria-label','analogBEAST Sequencer-Editor');
 panel.innerHTML=`<div class="abseq-wrap">
 <div class="abseq-top"><div><h1>analogBEAST v1.0 · SEQ EDITOR</h1><div class="abseq-muted">8 Bänke · A–D 64 Steps · E–H 16 Steps · 32 SONGs · 4 ALBUMs</div></div><button id="abseq-close" type="button">✕ Zurück zum Synth-Editor</button></div>
 <div class="abseq-bar"><button class="abseq-tab" data-mode="pattern" aria-pressed="true">PATTERNS A–H</button><button class="abseq-tab" data-mode="song" aria-pressed="false">SONG EDIT</button><button class="abseq-tab" data-mode="album" aria-pressed="false">ALBUM EDIT</button>
 <span class="abseq-right abseq-muted">MIDI SysEx 96</span></div>
 <div class="abseq-bar"><button id="abseq-connect" class="abseq-main">MIDI verbinden</button><label>IN <select id="abseq-in"></select></label><label>OUT <select id="abseq-out"></select></label><span class="abseq-muted">Gerät STOP, REC verlassen</span></div>
 <div class="abseq-bar" id="abseq-resource"></div>
 <div id="abseq-content"></div>
 <div class="abseq-bar"><button id="abseq-read" class="abseq-main">Vom Gerät LESEN</button><button id="abseq-export">JSON exportieren</button><button id="abseq-all">Alles sichern (164 Datensätze)</button><button id="abseq-import">JSON importieren</button><input id="abseq-file" type="file" accept=".json,application/json" hidden><button id="abseq-write">Mit BACKUP schreiben</button></div>
 <p class="abseq-warning">Schreiben nur nach automatischem Geräte-Backup und ausdrücklicher Bestätigung. Danach wird zurückgelesen und Autosave geprüft. Kein Flash-Vorgang. Niemals während PLAY/REC schreiben.</p>
 <div class="abseq-status" id="abseq-status" role="status">Bereit. Für Änderungen zuerst das Ziel vom Gerät lesen oder eine passende analogBEAST-v1.0-JSON-Datei laden.</div>
 </div>`;document.body.appendChild(panel);
 const $=id=>panel.querySelector('#'+id);
 const state={mode:'pattern',key:{bank:'A',pattern:1,song:1,album:1},page:0,draft:null,entries:null,dirty:false,proto:null,access:null,busy:false};
 const setStatus=(m)=>{$('abseq-status').textContent=m;};
 const option=(value,label,selected=false)=>`<option value="${value}"${selected?' selected':''}>${label}</option>`;
 const options=(n,sel,prefix='')=>Array.from({length:n},(_,i)=>option(i+1,prefix+(i+1),i+1===sel)).join('');
 const keyFor=()=>state.mode==='pattern'?{bank:state.key.bank,pattern:state.key.pattern}:state.mode==='song'?{song:state.key.song}:{album:state.key.album};
 function updatePanel(){
  panel.querySelectorAll('.abseq-tab').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.mode===state.mode)));
  const k=state.key;
  $('abseq-resource').innerHTML=state.mode==='pattern'?`<label>Bank <select data-key="bank">${[...BANKS].map(v=>option(v,v,v===k.bank)).join('')}</select></label><label>Pattern <select data-key="pattern">${options(16,k.pattern,'P')}</select></label><span>${k.bank<='D'?'64':'16'} physische Steps pro Pattern</span>`:
    state.mode==='song'?`<label>SONG <select data-key="song">${options(32,k.song,'SONG ')}</select></label><span>64 Song-Zeilen / 4 Seiten</span>`:
      `<label>ALBUM <select data-key="album">${options(4,k.album,'ALBUM ')}</select></label><span>32 Song-Slots pro Album</span>`;
  $('abseq-resource').querySelectorAll('[data-key]').forEach(el=>el.addEventListener('change',()=>{if(!allowDiscard()){el.value=state.key[el.dataset.key];return;}state.key[el.dataset.key]=el.dataset.key==='bank'?el.value:+el.value;clearDraft();}));
  render();
 }
 function allowDiscard(){return !state.dirty||confirm('Ungespeicherte Änderungen verwerfen?');}
 function clearDraft(){state.draft=null;state.entries=null;state.dirty=false;state.page=0;render();}
 function loaded(){return state.draft&&state.draftType===state.mode;}
 function mark(){state.dirty=true;setStatus('Änderungen nur im Browser (noch NICHT im Gerät).');}
 function render(){
  const target=$('abseq-content');
  if(state.entries){target.innerHTML=`<h2>Import-Vorschau: ${state.entries.length} Datensätze</h2><p>${state.entries.map(i=>keyOf(i.type,i.record)).join(' · ')}</p><p>Nach „Mit BACKUP schreiben“ werden vor dem ersten Schreibvorgang sämtliche Zieladressen gesichert.</p>`;return;}
  if(!loaded()){target.innerHTML=`<div class="abseq-bar abseq-muted">${state.mode.toUpperCase()} auswählen und „Vom Gerät LESEN“ drücken. Alternativ JSON importieren.</div>`;return;}
  const d=state.draft;
  if(state.mode==='pattern'){
   const n=d.steps.length,pages=n/16;state.page=Math.min(state.page,pages-1);
   target.innerHTML=`<div class="abseq-bar"><label>Länge <input id="abseq-length" type="number" min="1" max="${n}" value="${d.length}"></label><label>Resolution (Gerätewert) <input id="abseq-res" type="number" min="0" max="${n===64?9:5}" value="${d.resolution}"></label>
    <span class="abseq-right">${pages>1?`<button id="abseq-prev">◀</button> Seite ${state.page+1}/${pages} <button id="abseq-next">▶</button>`:`16 Steps`}</span></div>
    <div class="abseq-tablewrap"><table><thead><tr><th>Step</th><th>Note (0–127)</th><th>Flags (0–31)</th><th>Velocity</th><th>Gate</th><th>Swing</th></tr></thead><tbody>
    ${d.steps.slice(state.page*16,state.page*16+16).map((s,i)=>`<tr class="${state.page*16+i>=d.length?'abseq-inactive':''}"><td>${String(state.page*16+i+1).padStart(2,'0')}</td>${FIELDS.map((f)=>`<td><input class="abseq-sm" type="number" data-step="${state.page*16+i}" data-field="${f}" min="${f==='velocity'||f==='gate'?1:0}" max="${f==='flags'?31:f==='swing'?100:127}" value="${s[f]}"></td>`).join('')}</tr>`).join('')}</tbody></table></div><p class="abseq-muted">Flags: 1=Accent, 2=Slide, 4=Tie, Bits 8/16=Ratchet. Nicht geänderte Bits bleiben erhalten.</p>`;
   $('abseq-length').onchange=e=>{try{const v=+e.target.value;integer(v,1,n,'Länge');d.length=v;mark();render();}catch(err){setStatus(err.message);render();}};
   $('abseq-res').onchange=e=>{try{const v=+e.target.value;integer(v,0,n===64?9:5,'Resolution');d.resolution=v;mark();}catch(err){setStatus(err.message);render();}};
   if(pages>1){$('abseq-prev').onclick=()=>{state.page=(state.page+pages-1)%pages;render();};$('abseq-next').onclick=()=>{state.page=(state.page+1)%pages;render();};}
   target.querySelectorAll('[data-step]').forEach(el=>el.onchange=()=>{const v=+el.value,lo=['velocity','gate'].includes(el.dataset.field)?1:0,hi=el.dataset.field==='flags'?31:el.dataset.field==='swing'?100:127;
    try{integer(v,lo,hi,el.dataset.field);d.steps[+el.dataset.step][el.dataset.field]=v;mark();}catch(err){setStatus(err.message);render();}});
  }else if(state.mode==='song'){
   state.page=Math.min(state.page,3);
   const cmds=[['pattern','PATTERN'],['end','END'],['tempo','TEMPO'],['preset','PRESET'],['rest','REST'],['arp','ARP']];
   const kind=r=>r[0]<8?'pattern':({8:'end',9:'tempo',10:'preset',11:'rest',12:'arp'}[r[0]]);
   target.innerHTML=`<div class="abseq-bar"><label>Song-Zeilen <input id="abseq-count" type="number" min="1" max="64" value="${d.count}"></label><label>Modus <select id="abseq-loop">${option(0,'ONCE',d.loop===0)}${option(1,'LOOP',d.loop===1)}</select></label>
    <span class="abseq-right"><button id="abseq-prev">◀</button> Seite ${state.page+1}/4 <button id="abseq-next">▶</button></span></div>
    <div class="abseq-tablewrap"><table><thead><tr><th>Zeile</th><th>Befehl</th><th>Bank</th><th>Pattern</th><th>Steps / Pause</th><th>Wert</th></tr></thead><tbody>
    ${d.rows.slice(state.page*16,state.page*16+16).map((r,i)=>{const ix=state.page*16+i,k=kind(r),pat=k==='pattern',val=[9,10,12].includes(r[0]);return `<tr class="${ix>=d.count?'abseq-inactive':''}"><td>${String(ix+1).padStart(2,'0')}</td>
     <td><select data-row="${ix}" data-col="kind">${cmds.map(([v,l])=>option(v,l,v===k)).join('')}</select></td>
     <td><select data-row="${ix}" data-col="bank" ${pat?'':'disabled'}>${[...BANKS].map((v,b)=>option(b,v,r[0]===b)).join('')}</select></td>
     <td><input class="abseq-sm" type="number" data-row="${ix}" data-col="pattern" min="1" max="16" value="${pat?r[1]+1:1}" ${pat?'':'disabled'}></td>
     <td><input class="abseq-sm" type="number" data-row="${ix}" data-col="steps" min="1" max="${pat&&r[0]>=4?16:64}" value="${r[2]}" ${pat||k==='rest'?'':'disabled'}></td>
     <td><input class="abseq-sm" type="number" data-row="${ix}" data-col="value" min="${k==='tempo'?40:0}" max="${k==='tempo'?240:k==='preset'?31:k==='arp'?1:0}" value="${r[3]}" ${val?'':'disabled'}></td></tr>`;}).join('')}</tbody></table></div><p class="abseq-muted">END beendet den Song. TEMPO 40–240 BPM, PRESET 0–31, REST mit Steps, ARP 0/1.</p>`;
   $('abseq-count').onchange=e=>{try{integer(+e.target.value,1,64,'Zeilen');d.count=+e.target.value;mark();render();}catch(err){setStatus(err.message);render();}};
   $('abseq-loop').onchange=e=>{d.loop=+e.target.value;mark();};
   $('abseq-prev').onclick=()=>{state.page=(state.page+3)%4;render();};$('abseq-next').onclick=()=>{state.page=(state.page+1)%4;render();};
   target.querySelectorAll('[data-row]').forEach(el=>el.onchange=()=>{const ix=+el.dataset.row,col=el.dataset.col,r=d.rows[ix];try{
    if(col==='kind'){const typ=el.value;d.rows[ix]=typ==='pattern'?[0,0,16,0]:typ==='end'?[8,16,1,0]:typ==='tempo'?[9,0,1,120]:typ==='preset'?[10,0,1,0]:typ==='rest'?[11,0,16,0]:[12,0,1,1];}
    else if(col==='bank'){r[0]=+el.value;if(r[0]>=4)r[2]=Math.min(16,r[2]);}
    else if(col==='pattern'){integer(+el.value,1,16,'Pattern');r[1]=+el.value-1;}
    else if(col==='steps'){integer(+el.value,1,r[0]>=4&&r[0]<=7?16:64,'Steps');r[2]=+el.value;}
    else if(col==='value'){let lo=r[0]===9?40:0,hi=r[0]===9?240:r[0]===10?31:r[0]===12?1:0;integer(+el.value,lo,hi,'Wert');r[3]=+el.value;}
    validateRow(d.rows[ix]);mark();render();
   }catch(err){setStatus(err.message);render();}});
  }else{
   target.innerHTML=`<div class="abseq-bar"><label>Song-Slots <input id="abseq-count" type="number" min="0" max="32" value="${d.count}"></label><label>Modus <select id="abseq-loop">${option(0,'ONCE',d.loop===0)}${option(1,'LOOP',d.loop===1)}</select></label></div>
    <div class="abseq-tablewrap"><table><thead><tr><th>Slot</th><th>SONG</th><th>Status</th></tr></thead><tbody>
    ${d.song_ids.map((sid,i)=>`<tr class="${i>=d.count?'abseq-inactive':''}"><td>${String(i+1).padStart(2,'0')}</td><td><select data-slot="${i}" ${i<d.count?'':'disabled'}>${options(32,sid||1,'SONG ')}</select></td><td>${i<d.count?'PLAY':'END / leer'}</td></tr>`).join('')}</tbody></table></div>`;
   $('abseq-count').onchange=e=>{try{const count=+e.target.value;integer(count,0,32,'Slots');for(let i=0;i<32;i++){if(i<count&&d.song_ids[i]===null)d.song_ids[i]=1;if(i>=count)d.song_ids[i]=null;}d.count=count;mark();render();}catch(err){setStatus(err.message);render();}};
   $('abseq-loop').onchange=e=>{d.loop=+e.target.value;mark();};
   target.querySelectorAll('[data-slot]').forEach(el=>el.onchange=()=>{d.song_ids[+el.dataset.slot]=+el.value;mark();});
  }
 }
 async function busyAction(fn){if(state.busy)return;state.busy=true;for(const el of panel.querySelectorAll('button'))el.disabled=true;
  try{await fn();}catch(e){console.error('analogBEAST SEQ:',e);setStatus('FEHLER: '+e.message);}
  finally{state.busy=false;for(const el of panel.querySelectorAll('button'))el.disabled=false;}}
 function mustMIDI(){if(!state.proto)throw Error('Zuerst MIDI verbinden');return state.proto;}
 function download(name,obj){const blob=new Blob([JSON.stringify(obj,null,2)+'\n'],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=name;document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),60000);}
 function timeName(){return new Date().toISOString().replace(/[:.]/g,'-');}
 async function connect(){if(!navigator.requestMIDIAccess)throw Error('WebMIDI nicht verfügbar – Chrome/Edge auf localhost oder HTTPS verwenden');
  state.access=await navigator.requestMIDIAccess({sysex:true});const ins=[...state.access.inputs.values()],outs=[...state.access.outputs.values()];
  for(const [id,ports] of [['abseq-in',ins],['abseq-out',outs]]){$(id).innerHTML=ports.map(p=>option(p.id,p.name||p.id,false)).join('');
   const preferred=ports.filter(p=>/analogbeast/i.test(p.name||''));if(preferred.length===1)$(id).value=preferred[0].id;}
  const reconnect=()=>{const inp=state.access.inputs.get($('abseq-in').value),out=state.access.outputs.get($('abseq-out').value);
   if(!inp||!out){state.proto=null;return;}state.proto=new Protocol(out,inp);setStatus(`MIDI verbunden: IN=${inp.name} | OUT=${out.name}. Bitte STOP und REC verlassen.`);};
  $('abseq-in').onchange=reconnect;$('abseq-out').onchange=reconnect;reconnect();
 }
 $('abseq-connect').onclick=()=>busyAction(connect);
 $('abseq-read').onclick=()=>busyAction(async()=>{if(!allowDiscard())return;const p=mustMIDI();const d=await p.read(state.mode,keyFor());state.draft=clone(d);state.draftType=state.mode;state.dirty=false;state.entries=null;state.page=0;render();setStatus('Vom Gerät gelesen: '+keyOf(state.mode,d)+' (nur Browser, nicht verändert).');});
 $('abseq-export').onclick=()=>{if(!loaded()&&!state.entries){setStatus('Erst Daten lesen oder importieren.');return;}try{
  const items=state.entries||[{type:state.mode,record:state.draft}];items.forEach(i=>checkRecord(i.type,i.record));download(`analogBEAST-v1-${timeName()}.json`,docOf(items));setStatus('JSON-Export im Browser gestartet.');
 }catch(e){setStatus(e.message);}};
 $('abseq-all').onclick=()=>busyAction(async()=>{
  const p=mustMIDI();if(!confirm('128 Patterns, 32 SONGs und 4 ALBUMs nur LESEN und als JSON sichern?\nDas kann einige Zeit dauern. Kein Schreibvorgang.'))return;
  const list=[];for(const bank of BANKS)for(let pattern=1;pattern<=16;pattern++)list.push({type:'pattern',key:{bank,pattern}});
  for(let song=1;song<=32;song++)list.push({type:'song',key:{song}});
  for(let album=1;album<=4;album++)list.push({type:'album',key:{album}});
  const records=[];for(let i=0;i<list.length;i++){
   setStatus(`Gesamtbackup: ${i+1}/${list.length} – lese ${list[i].type} ...`);
   const entry=list[i],record=await p.read(entry.type,entry.key);records.push({type:entry.type,record});
  }
  const name=`analogBEAST-FULL-BACKUP-${timeName()}.json`;download(name,docOf(records));
  setStatus(`PASS: ${list.length} Datensätze gelesen. Gesamtbackup-Download: ${name} – Speicherort im Browser prüfen.`);
 });
 $('abseq-import').onclick=()=>{$('abseq-file').value='';$('abseq-file').click();};
 $('abseq-file').onchange=async e=>{try{const file=e.target.files[0];if(!file)return;if(!allowDiscard())return;const parsed=parseDocument(JSON.parse(await file.text()));state.dirty=false;state.page=0;
   if(parsed.length===1){const item=parsed[0],r=item.record;state.mode=item.type;state.draft=clone(r);state.draftType=item.type;state.entries=null;
    Object.assign(state.key,item.type==='pattern'?{bank:r.bank,pattern:r.pattern}:item.type==='song'?{song:r.song}:{album:r.album});updatePanel();setStatus('Import-Vorschau: '+keyOf(item.type,r)+' (noch nicht ans Gerät gesendet).');}
   else {state.entries=parsed;state.draft=null;updatePanel();setStatus(`Import-Vorschau: ${parsed.length} Datensätze. Vor dem Schreiben werden ALLE Ziele gesichert.`);}
  }catch(err){setStatus('JSON-Importfehler: '+err.message);}};
 $('abseq-write').onclick=()=>busyAction(async()=>{
  const items=state.entries||loaded()?[...(state.entries||[{type:state.mode,record:clone(state.draft)}])]:null;
  if(!items||!items.length)throw Error('Erst Datensatz laden/lesen');
  items.forEach(i=>checkRecord(i.type,i.record));const p=mustMIDI();
  setStatus(`Lese vor dem Schreiben ${items.length} Ziel(e) vom Gerät für Backup ...`);
  const old=[];for(let i=0;i<items.length;i++){const entry=items[i],r=await p.read(entry.type,entry.record);old.push({type:entry.type,record:r});setStatus(`Backup lese ${i+1}/${items.length} ...`);}
  const name=`analogBEAST-BACKUP-before-write-${timeName()}.json`;download(name,docOf(old));
  if(!confirm(`BACKUP-DOWNLOAD: ${name}\n\nBitte prüfen, dass die Datei wirklich gespeichert wurde. Danach ${items.length} Ziel(e) am FM-1 überschreiben? STOP/REC verlassen!`)){
   setStatus('Schreiben abgebrochen; das Backup wurde zum Download angeboten.');return;
  }
  for(let i=0;i<items.length;i++){const entry=items[i];setStatus(`Schreibe ${i+1}/${items.length}: ${keyOf(entry.type,entry.record)}`);
   await p.write(entry.type,entry.record);const actual=await p.read(entry.type,entry.record);
   if(!eq(actual,entry.record))throw Error(`Rückleseprüfung fehlgeschlagen: ${keyOf(entry.type,entry.record)}. Backup: ${name}`);
  }
  await p.waitSaved(items,setStatus);state.dirty=false;state.entries=null;
  setStatus(`PASS: ${items.length} Ziel(e) geschrieben, vollständig zurückgelesen, Autosave bestätigt. Backup: ${name}`);
 });
 panel.querySelectorAll('.abseq-tab').forEach(btn=>btn.onclick=()=>{if(btn.dataset.mode===state.mode)return;if(!allowDiscard())return;state.mode=btn.dataset.mode;clearDraft();updatePanel();});
 open.onclick=()=>{panel.classList.add('abseq-visible');open.hidden=true;};
 $('abseq-close').onclick=()=>{panel.classList.remove('abseq-visible');open.hidden=false;};
 updatePanel();
}
if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',init,{once:true});else init();
})(typeof window!=='undefined'?window:globalThis);
