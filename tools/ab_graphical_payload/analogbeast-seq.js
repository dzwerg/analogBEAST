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
  integer(r.length,1,n,'Patternlänge');integer(r.resolution,0,9,'Auflösung');
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
 <span class="abseq-right" style="display:flex;gap:6px;align-items:center"><button id="abseq-undo" title="Undo (Strg+Z)">↶ Undo</button><button id="abseq-redo" title="Redo (Strg+Y)">↷ Redo</button><span class="abseq-muted">MIDI SysEx 96</span></span></div>
 <div class="abseq-bar"><button id="abseq-connect" class="abseq-main">MIDI verbinden / Aktualisieren</button><label>IN <select id="abseq-in"><option>— kein Port —</option></select></label><label>OUT <select id="abseq-out"><option>— kein Port —</option></select></label><span class="abseq-muted">Gerät STOP, REC verlassen · WebMIDI + SysEx erforderlich</span></div>
 <div class="abseq-bar" id="abseq-resource"></div>
 <div id="abseq-content"></div>
 <div class="abseq-bar"><button id="abseq-read" class="abseq-main">Vom Gerät LESEN</button><button id="abseq-export">JSON exportieren</button><button id="abseq-all">Alles sichern (164 Datensätze)</button><button id="abseq-import">JSON importieren</button><input id="abseq-file" type="file" accept=".json,application/json" hidden><button id="abseq-write">Mit BACKUP schreiben</button></div>
 <p class="abseq-warning">Schreiben nur nach automatischem Geräte-Backup und ausdrücklicher Bestätigung. Danach wird zurückgelesen und Autosave geprüft. Kein Flash-Vorgang. Niemals während PLAY/REC schreiben.</p>
 <div class="abseq-status" id="abseq-status" role="status">Bereit. Für Änderungen zuerst das Ziel vom Gerät lesen oder eine passende analogBEAST-v1.0-JSON-Datei laden.</div>
 </div>`;document.body.appendChild(panel);
 const $=id=>panel.querySelector('#'+id);
 const state={mode:'pattern',key:{bank:'A',pattern:1,song:1,album:1},page:0,draft:null,entries:null,dirty:false,proto:null,access:null,busy:false,selectedStep:0,selectedRow:0,rollX:0,rollY:260,previewStep:-1,history:[],future:[],lastDraft:null};
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
 function clearDraft(){stopPreview();state.draft=null;state.entries=null;state.dirty=false;state.history=[];state.future=[];state.lastDraft=null;state.page=0;state.selectedStep=0;state.rollX=0;state.rollY=260;render();}
 function loaded(){return state.draft&&state.draftType===state.mode;}
 function mark(){state.dirty=true;setStatus('Änderungen nur im Browser (noch NICHT im Gerät).');}
 // --- Graphical editor: one monophonic event per FM-1 sequencer step. ---
 const NOTE_NAMES=['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B'];
 const noteName=n=>n===0?'REST':`${NOTE_NAMES[n%12]}${Math.floor(n/12)-1}`;
 const stepKind=s=>(s.flags&4)?'TIE':s.note===0?'REST':'NOTE';
 const newRecord=(mode,key)=>mode==='pattern'?{bank:key.bank,pattern:key.pattern,length:key.bank<='D'?64:16,resolution:2,steps:Array.from({length:key.bank<='D'?64:16},DEFAULT_STEP)}:
  mode==='song'?{song:key.song,count:1,loop:0,rows:Array.from({length:64},()=>[8,16,1,0])}:
  {album:key.album,count:0,loop:0,song_ids:Array(32).fill(null)};
// Minimal Standard MIDI File handling for one monophonic FM-1 pattern.
// MIDI import is deliberately non-destructive: it creates a local pattern draft only.
function midiVq(v){let out=[v&127];v>>=7;while(v){out.unshift((v&127)|128);v>>=7;}return out;}
function midiSmfNotes(bytes){
 const a=bytes instanceof Uint8Array?bytes:new Uint8Array(bytes);
 const chk=(i,n=1)=>{if(i<0||i+n>a.length)throw Error('MIDI-Datei ist unvollständig');};
 const u16=i=>{chk(i,2);return (a[i]<<8)|a[i+1];};
 const u32=i=>{chk(i,4);return (a[i]*16777216)+(a[i+1]<<16)+(a[i+2]<<8)+a[i+3];};
 const word=(i,n)=>{chk(i,n);return String.fromCharCode(...a.slice(i,i+n));};
 if(a.length<14||word(0,4)!=='MThd')throw Error('Kein Standard MIDI File (.mid)');
 const hsz=u32(4),type=u16(8),ntracks=u16(10),division=u16(12);
 if(hsz<6||type>1||(division&0x8000)||!division)throw Error('Nur MIDI Typ 0/1 mit PPQ-Ticks unterstützt');
 if(ntracks>1000)throw Error('Zu viele MIDI-Tracks');
 let p=8+hsz;const active=new Map(),notes=[];
 const takeVq=(end)=>{let v=0;for(let j=0;j<4;j++){if(p>=end)throw Error('Ungültiger MIDI-Zeitwert');const c=a[p++];v=(v<<7)|(c&127);if(!(c&128))return v;}throw Error('MIDI-Zeitwert zu lang');};
 for(let tr=0;tr<ntracks;tr++){
  if(word(p,4)!=='MTrk')throw Error('MIDI-Track fehlt');const len=u32(p+4);p+=8;const end=p+len;chk(p,len);
  let tick=0,run=0;
  while(p<end){tick+=takeVq(end);if(p>=end)throw Error('MIDI-Track abgeschnitten');let status=a[p++];
   if(status<128){if(!run)throw Error('Fehlerhafter MIDI-Running-Status');p--;status=run;}
   else if(status<240)run=status;else if(status!==0xF8&&status!==0xFA)run=0;
   if(status===0xff){chk(p,1);p++;const mlen=takeVq(end);if(p+mlen>end)throw Error('MIDI-Metaevent beschädigt');p+=mlen;continue;}
   if(status===0xf0||status===0xf7){const slen=takeVq(end);if(p+slen>end)throw Error('MIDI-SysEx beschädigt');p+=slen;continue;}
   if(status>=240){const sizes={0xF1:1,0xF2:2,0xF3:1,0xF6:0,0xF8:0,0xFA:0,0xFB:0,0xFC:0,0xFE:0};p+=sizes[status]??0;continue;}
   const t=status&240,ch=status&15,size=(t===0xc0||t===0xd0)?1:2;
   if(p+size>end)throw Error('MIDI-Nachricht abgeschnitten');const note=a[p++],vel=size===2?a[p++]:0;
   if(t===0x90&&vel){const key=tr+':'+ch+':'+note;const arr=active.get(key)||[];arr.push({tick,note,vel,ch});active.set(key,arr);}
   else if(t===0x80||(t===0x90&&vel===0)){
    const key=tr+':'+ch+':'+note,arr=active.get(key);
    if(arr&&arr.length){const start=arr.shift();notes.push({...start,duration:Math.max(1,tick-start.tick)});}
   }
  }
  p=end;
 }
 return {notes,division};
}
function midiToPattern(bytes,bank,pattern,channel=null){
 const {notes,division}=midiSmfNotes(bytes),n=bank<='D'?64:16,stepTicks=division/4;
 // Prefer the melodic channel with the most Note-On messages; skip GM drum channel 10.
 const stats=new Map();for(const nt of notes)if(nt.ch!==9)stats.set(nt.ch,(stats.get(nt.ch)||0)+1);
 const picked=channel!==null?channel:[...stats.entries()].sort((a,b)=>b[1]-a[1])[0]?.[0];
 if(picked===undefined)throw Error('Keine melodischen MIDI-Noten gefunden');
 const rec=newRecord('pattern',{bank,pattern});let filled=0,discard=0;
 const candidates=notes.filter(nt=>nt.ch===picked).sort((a,b)=>a.tick-b.tick||b.vel-a.vel);
 for(const nt of candidates){const i=Math.round(nt.tick/stepTicks);
  if(i<0||i>=n){discard++;continue;}
  const st=rec.steps[i];if(st.note!==0){discard++;continue;}st.note=nt.note;st.flags=0;st.velocity=Math.max(1,Math.min(127,nt.vel));st.gate=Math.max(1,Math.min(127,Math.round(nt.duration/stepTicks*127)));filled++;
 }
 if(!filled)throw Error(`Keine Noten in den ersten ${n} Sechzehnteln gefunden`);
 rec.length=Math.min(n,Math.max(1,candidates.reduce((mx,nt)=>Math.max(mx,Math.round(nt.tick/stepTicks)+1),1)));
 return {record:rec,filled,discard,channel:picked+1,total:candidates.length};
}
function patternToMidi(r){
 checkRecord('pattern',r);const division=480,unit=division/4;let ev=[];
 for(let i=0;i<r.length;i++){const st=r.steps[i];if(!st.note||(st.flags&4))continue;
  let end=i+1;while(end<r.length&&(r.steps[end].flags&4))end++;
  const start=i*unit,duration=Math.max(1,Math.round(unit*(st.gate/127))+(end-i-1)*unit);
  ev.push({tick:start,bytes:[0x90,st.note,st.velocity]},{tick:start+duration,bytes:[0x80,st.note,0]});
 }
 ev.sort((a,b)=>a.tick-b.tick||a.bytes[0]-b.bytes[0]);
 const data=[],push=(dt,msg)=>{data.push(...midiVq(dt),...msg);};push(0,[0xFF,0x51,3,0x07,0xA1,0x20]); // 120 BPM
 let time=0;for(const e of ev){push(e.tick-time,e.bytes);time=e.tick;}
 push(0,[0xFF,0x2F,0]);const be32=n=>[(n>>>24)&255,(n>>>16)&255,(n>>>8)&255,n&255];
 return new Uint8Array([...BufferlessASCII('MThd'),0,0,0,6,0,0,0,1,1,224,...BufferlessASCII('MTrk'),...be32(data.length),...data]);
}
function BufferlessASCII(s){return [...s].map(c=>c.charCodeAt(0));}

 root.ABSEQ.noteName=noteName;root.ABSEQ.stepKind=stepKind;root.ABSEQ.newRecord=newRecord;root.ABSEQ.midiSmfNotes=midiSmfNotes;root.ABSEQ.midiToPattern=midiToPattern;root.ABSEQ.patternToMidi=patternToMidi;
 const fmt=n=>String(n).padStart(2,'0');
 const cmdName=b=>b<8?'PATTERN':({8:'END',9:'TEMPO',10:'PRESET',11:'REST',12:'ARP'}[b]||'?');
 const rowTitle=r=>r[0]<8?`${BANKS[r[0]]}${fmt(r[1]+1)} · ${r[2]} Steps`:
  r[0]===8?'END':r[0]===9?`${r[3]} BPM`:r[0]===10?`Preset ${r[3]+1}`:r[0]===11?`Pause ${r[2]} Steps`:r[3]?'ARP ON':'ARP OFF';
 let previewStop=null,previewCtx=null;
 let rollDrag=null;
 function stopPreview(){if(previewStop){previewStop();previewStop=null;}state.previewStep=-1;}
 function setDirty(){
  if(state.draft&&state.lastDraft&&!eq(state.draft,state.lastDraft)){
   state.history.push(clone(state.lastDraft));if(state.history.length>60)state.history.shift();
   state.future=[];state.lastDraft=clone(state.draft);
  }
  mark();syncHistoryButtons();
 }
 function syncHistoryButtons(){const u=$('abseq-undo'),r=$('abseq-redo');if(u)u.disabled=state.history.length===0;if(r)r.disabled=state.future.length===0;}
 function useHistory(direction){const source=direction==='undo'?state.history:state.future,other=direction==='undo'?state.future:state.history;
  if(!source.length||!loaded())return;other.push(clone(state.draft));state.draft=source.pop();state.lastDraft=clone(state.draft);
  state.dirty=true;render();syncHistoryButtons();setStatus(direction==='undo'?'Undo – nur im Browser.':'Redo – nur im Browser.');
 }

 function setDraft(mode,rec,source){state.mode=mode;state.draft=clone(rec);state.draftType=mode;state.entries=null;state.dirty=false;state.page=0;state.selectedStep=0;state.rollX=0;state.rollY=260;state.history=[];state.future=[];state.lastDraft=clone(state.draft);stopPreview();updatePanel();syncHistoryButtons();setStatus(source);}
 function ensureStyles(){if(document.getElementById('abseq-v2-style'))return;
  const css=document.createElement('style');css.id='abseq-v2-style';css.textContent=`
  #abseq-panel{background:#0e1015;font:13px/1.5 ui-sans-serif,system-ui,Segoe UI,sans-serif;color:#dce1eb}
  #abseq-panel .abseq-wrap{max-width:1510px;padding:24px 30px 70px}
  #abseq-panel h1{font-size:24px;letter-spacing:-.6px} #abseq-panel h2{font-size:17px;margin:0 0 6px;color:#f1f3f8}
  #abseq-panel .abseq-bar{background:#1a1f29;border:1px solid #2b3341;border-radius:10px;padding:12px 14px}
  #abseq-panel button,#abseq-panel select,#abseq-panel input{background:#10151f;border:1px solid #343d4f;color:#e6eaf1;border-radius:6px;padding:7px 9px}
  #abseq-panel button:hover,#abseq-panel select:hover{border-color:#ffab57}
  #abseq-panel .abseq-main,#abseq-panel .abseq-tab[aria-pressed=true]{background:#b76314;color:white;border-color:#f5a34b}
  #abseq-panel button:disabled{opacity:.48;cursor:not-allowed}
  #abseq-panel .abseq-titlebar{display:flex;gap:14px;align-items:center;flex-wrap:wrap;padding:16px 0 8px}
  #abseq-panel .abseq-subtle{font-size:12px;color:#92a0b5}
  #abseq-panel .abseq-grid{display:grid;grid-template-columns:minmax(0,1fr) 290px;gap:14px}
  #abseq-panel .abseq-card{border:1px solid #313a49;background:#171c26;border-radius:12px;padding:14px;min-width:0}
  #abseq-panel .abseq-inspector label{display:flex;align-items:center;justify-content:space-between;gap:8px;margin:9px 0}
  #abseq-panel .abseq-inspector input[type=number]{width:90px}
  #abseq-panel .abseq-notehead{display:flex;gap:12px;align-items:center;flex-wrap:wrap;justify-content:space-between;margin-bottom:12px}
  #abseq-panel .abseq-pianoscroll{max-height:520px;overflow:auto;border:1px solid #30394a;border-radius:8px;background:#10151e;position:relative;scrollbar-color:#4e5a6a #171c26}
  #abseq-panel .abseq-pianoscroll svg{display:block;max-width:none;touch-action:pan-x pan-y;user-select:none}
  #abseq-panel .abseq-pianoscroll svg[data-roll]{cursor:crosshair;touch-action:none}
  #abseq-panel .abseq-velscroll{overflow-x:auto;background:#121720;border:1px solid #30394a;border-radius:7px;margin-top:8px}
  #abseq-panel .abseq-stepbuttons{display:grid;grid-template-columns:repeat(16,minmax(31px,1fr));gap:5px;margin:8px 0 12px}
  #abseq-panel .abseq-stepbuttons button{padding:6px 2px;font-size:11px}
  #abseq-panel .abseq-stepbuttons button[data-has=true]{background:#613d20;border-color:#cd842e}
  #abseq-panel .abseq-stepbuttons button[data-active=true]{outline:2px solid #ffb158;outline-offset:1px}
  #abseq-panel .abseq-notelegend{display:flex;gap:12px;flex-wrap:wrap;color:#94a1b2;font-size:12px}
  #abseq-panel .abseq-chip{border:1px solid #3d4656;border-radius:8px;background:#222936;padding:8px 10px;display:flex;align-items:center;gap:8px}
  #abseq-panel .abseq-cmdstrip{display:flex;align-items:stretch;gap:5px;overflow:auto;padding:14px 2px;scrollbar-color:#3d4656 transparent}
  #abseq-panel .abseq-cmd{border-radius:6px;min-width:92px;flex:0 0 auto;display:flex;flex-direction:column;align-items:flex-start;gap:3px;text-align:left}
  #abseq-panel .abseq-cmd[data-type=PATTERN]{background:#60411f;border-color:#bc7f31}
  #abseq-panel .abseq-cmd[data-type=TEMPO]{background:#243d5e;border-color:#416a9b}
  #abseq-panel .abseq-cmd[data-type=PRESET]{background:#373259;border-color:#6e63a5}
  #abseq-panel .abseq-cmd[data-type=REST]{background:#30343c;border-color:#656a76}
  #abseq-panel .abseq-cmd[data-type=END]{background:#49303c;border-color:#945473}
  #abseq-panel .abseq-cmd[data-type=ARP]{background:#234c49;border-color:#397b75}
  #abseq-panel .abseq-cmd[data-chosen=true]{outline:2px solid #fff1d3}
  #abseq-panel .abseq-cmd small{font-size:10px;opacity:.75}
  #abseq-panel .abseq-songrows{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:7px}
  #abseq-panel .abseq-songrow{padding:8px;border:1px solid #313947;background:#1b222d;border-radius:8px}
  #abseq-panel .abseq-songrow[data-active=true]{border-color:#e29a46}
  #abseq-panel .abseq-rowhead{display:flex;align-items:center;gap:8px;justify-content:space-between}
  #abseq-panel .abseq-songrow .abseq-rowfields{display:flex;align-items:center;gap:7px;flex-wrap:wrap;margin-top:7px}
  #abseq-panel .abseq-rowfields label{display:inline-flex;align-items:center;gap:5px}
  #abseq-panel .abseq-rowfields input{width:67px}
  #abseq-panel .abseq-albumgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(218px,1fr));gap:10px}
  #abseq-panel .abseq-albumslot{padding:12px;border:1px solid #343d4b;border-radius:9px;background:#202733;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
  #abseq-panel .abseq-albumslot select{width:115px}
  #abseq-panel .abseq-albumslot[data-empty=true]{opacity:.45}
  #abseq-panel .abseq-albumslot .abseq-sm-btn{padding:5px 8px}
  #abseq-panel .abseq-notloaded{min-height:260px;display:flex;align-items:center;justify-content:center;flex-direction:column;gap:13px;text-align:center}
  #abseq-panel .abseq-notevalue{font-size:24px;font-weight:700;color:#ffb35b}
  #abseq-panel .abseq-tag{display:inline-block;color:#fbb865;background:#422b16;padding:3px 8px;border:1px solid #986323;border-radius:5px;font-size:11px}
  #abseq-panel .abseq-smallbtn{padding:5px 7px}
  @media(max-width:940px){#abseq-panel .abseq-grid{grid-template-columns:1fr}#abseq-panel .abseq-wrap{padding:14px}#abseq-panel .abseq-inspector{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:8px}}
 `;document.head.appendChild(css);
 }
 function syncScroll(){const sc=$('abseq-roll-scroll');if(sc){sc.scrollLeft=state.rollX||0;sc.scrollTop=state.rollY??230;sc.onscroll=()=>{state.rollX=sc.scrollLeft;state.rollY=sc.scrollTop;};}}
 function drawRoll(d){
  const n=d.steps.length,stepW=n===64?29:42,lo=36,hi=95,rowH=15,left=71,total=left+n*stepW,h=(hi-lo+1)*rowH+27;
  let svg=`<svg data-roll="1" width="${total}" height="${h}" viewBox="0 0 ${total} ${h}" role="img" aria-label="Piano-Roll: horizontale Steps, vertikale Tonhöhe. Klick setzt eine Note, Rechtsklick löscht.">`;
  for(let pitch=hi;pitch>=lo;pitch--){const y=(hi-pitch)*rowH+26,black=[1,3,6,8,10].includes(pitch%12);svg+=`<rect x="0" y="${y}" width="${total}" height="${rowH}" fill="${black?'#151b26':'#1c2230'}"/>`;
   svg+=`<rect x="0" y="${y}" width="${left-2}" height="${rowH}" fill="${black?'#303747':'#bec7d7'}"/>`;
   if(pitch%12===0)svg+=`<text x="8" y="${y+11}" font-size="11" fill="#1a212e">${noteName(pitch)}</text>`;
   svg+=`<path d="M${left} ${y}H${total}" stroke="${pitch%12===0?'#424b5d':'#273141'}" stroke-width=".7"/>`;
  }
  for(let i=0;i<=n;i++){let x=left+i*stepW;svg+=`<line x1="${x}" x2="${x}" y1="25" y2="${h-1}" stroke="${i%16===0?'#778194':i%4===0?'#4c5668':'#2a3544'}" stroke-width="${i%16===0?1.5:.75}"/>`;
   if(i<n&&i%4===0)svg+=`<text x="${x+3}" y="16" font-size="11" fill="${i%16===0?'#f2c288':'#9da9bc'}">${i+1}</text>`;
   if(i<d.length&&i%4===0)svg+=`<rect x="${x}" y="20" width="${stepW*4}" height="5" fill="#c98836" opacity=".45"/>`;
  }
  for(let i=0;i<n;i++){
   if(i>=d.length)svg+=`<rect x="${left+i*stepW}" y="26" width="${stepW}" height="${h-26}" fill="#101319" opacity=".6" pointer-events="none"/>`;
   const s=d.steps[i],type=stepKind(s);if(type==='REST')continue;
   if(type==='TIE'){
    const y=(hi-60)*rowH+26;svg+=`<rect x="${left+i*stepW+3}" y="${y}" width="${stepW-6}" height="${rowH-1}" rx="3" fill="#58859c" opacity=".85"/>`;continue;
   }
   if(s.note<lo||s.note>hi)continue;
   const y=(hi-s.note)*rowH+26;
   const color=(s.flags&1)?'#ffe3a7':'#f1a53f';
   const w=Math.max(7,(stepW-4)*(s.gate/127));
   svg+=`<rect data-drawn-step="${i}" x="${left+i*stepW+2}" y="${y+1}" width="${w.toFixed(1)}" height="${rowH-2}" rx="2" fill="${color}" stroke="${i===state.selectedStep?'#ffffff':'#ad721f'}" stroke-width="${i===state.selectedStep?2:1}"/>`;
  }
  svg+=`</svg>`;
  return {svg,left,lo,hi,rowH,stepW};
 }
 function rollPosition(e,info,elem){const rc=elem.getBoundingClientRect();const x=e.clientX-rc.left,y=e.clientY-rc.top;
  if(x<info.left||y<26)return null;
  const step=Math.floor((x-info.left)/info.stepW),pitch=info.hi-Math.floor((y-26)/info.rowH);
  if(step<0||step>=state.draft.steps.length||pitch<info.lo||pitch>info.hi)return null;
  return {step,pitch};}
 function bindPiano(d,info){const elem=$('abseq-piano');elem.oncontextmenu=e=>e.preventDefault();
  elem.onpointerdown=e=>{
   if(e.button!==0&&e.button!==2)return;
   const pos=rollPosition(e,info,elem);if(!pos)return;
   state.selectedStep=pos.step;
   rollDrag={start:pos,button:e.button,origin:clone(d.steps[pos.step])};
   if(e.button===0){try{elem.setPointerCapture(e.pointerId);}catch(_){}}
   e.preventDefault();
  };
  elem.onpointerup=e=>{if(!rollDrag)return;const drag=rollDrag;rollDrag=null;
   const pos=rollPosition(e,info,elem)||drag.start;
   const s=d.steps[drag.start.step];
   if(drag.button===2){s.note=0;s.flags&=~4;state.selectedStep=drag.start.step;setDirty();render();return;}
   if(pos.step!==drag.start.step){
    const from=clone(s);if(stepKind(from)!=='REST'){const to=d.steps[pos.step];if(stepKind(to)!=='REST'&&!confirm(`Step ${pos.step+1} enthält bereits ${noteName(to.note)}. Überschreiben?`)){render();return;}
     d.steps[pos.step]={...from,note:pos.pitch,flags:from.flags&~4};d.steps[drag.start.step]={...DEFAULT_STEP()};}
    else d.steps[pos.step]={...d.steps[pos.step],note:pos.pitch,flags:d.steps[pos.step].flags&~4};
    state.selectedStep=pos.step;
   }else{ s.note=pos.pitch;s.flags&=~4;state.selectedStep=pos.step; }
   setDirty();render();
  };
  elem.onpointercancel=()=>{rollDrag=null;};
  elem.ondblclick=e=>{const pos=rollPosition(e,info,elem);if(!pos)return;const s=d.steps[pos.step];s.note=0;s.flags&=~4;state.selectedStep=pos.step;setDirty();render();};
 }
 function render(){
  ensureStyles();const target=$('abseq-content');stopPreviewIfWrong();syncHistoryButtons();
  if(state.entries){const counts={pattern:0,song:0,album:0};state.entries.forEach(i=>counts[i.type]++);
   target.innerHTML=`<div class="abseq-card"><h2>JSON-Import · Vorschau</h2><p>${state.entries.length} Datensätze: ${counts.pattern} Patterns, ${counts.song} Songs und ${counts.album} Alben.</p><p class="abseq-subtle">Vor jedem Schreiben werden alle betroffenen Zieladressen vom FM-1 gesichert. Bei großen Importen ist die Speicherung nicht atomar.</p></div>`;return;}
  if(!loaded()){
   target.innerHTML=`<div class="abseq-card abseq-notloaded"><div class="abseq-tag">${state.mode==='pattern'?'PIANO-ROLL':state.mode==='song'?'SONG ARRANGER':'ALBUM PLAYLIST'}</div><h2>Bereit für ${state.mode==='pattern'?'Pattern A–H':state.mode==='song'?'32 Songs':'4 Alben'}</h2><div class="abseq-subtle">Vom FM-1 lesen, eine JSON-Datei importieren oder ohne Gerät neu beginnen.</div><button id="abseq-new" class="abseq-main">+ Leeres ${state.mode==='pattern'?'Pattern':state.mode==='song'?'Song':'Album'} erstellen (nur Browser)</button></div>`;
   $('abseq-new').onclick=()=>setDraft(state.mode,newRecord(state.mode,keyFor()),'Neuer, ungespeicherter Entwurf im Browser. Noch nichts am FM-1 geändert.');return;
  }
  if(state.mode==='pattern')renderPattern(target,state.draft);
  else if(state.mode==='song')renderSong(target,state.draft);
  else renderAlbum(target,state.draft);
 }
 function stopPreviewIfWrong(){if(!loaded()&&previewStop)stopPreview();}
 function renderPattern(target,d){
  const n=d.steps.length,idx=Math.min(n-1,state.selectedStep||0),step=d.steps[idx];state.selectedStep=idx;
  const info=drawRoll(d),pageN=Math.floor(idx/16),extra=n===64?' · 4 × 16':' · 16';
  const cards=d.steps.slice(pageN*16,pageN*16+16).map((s,j)=>{const i=pageN*16+j;return `<button data-pickstep="${i}" data-has="${stepKind(s)!=='REST'}" data-active="${idx===i}">${fmt(i+1)}<span style="display:block;color:#f6ae51;font-weight:700">${stepKind(s)==='NOTE'?noteName(s.note):stepKind(s)==='TIE'?'TIE':'–'}</span></button>`;}).join('');
  target.innerHTML=`<div class="abseq-titlebar"><div><h2>Bank ${d.bank} · Pattern ${fmt(d.pattern)} <span class="abseq-tag">${n} STEPS${extra}</span></h2><div class="abseq-subtle">Mono-Piano-Roll · Klick setzt Note · Ziehen verschiebt · Rechtsklick löscht · Doppelklick setzt REST</div></div>
   <div style="margin-left:auto;display:flex;align-items:center;gap:8px;flex-wrap:wrap"><button id="abseq-audition">▶ Browser-Vorhören</button><button id="abseq-midi-import">MIDI importieren</button><input id="abseq-mid-file" type="file" accept=".mid,.midi,audio/midi" hidden><button id="abseq-midi-export">MIDI exportieren</button><button id="abseq-clear-pat">Pattern leeren</button></div></div>
   <div class="abseq-grid"><div class="abseq-card">
    <div class="abseq-notehead"><div>PIANO ROLL <span class="abseq-subtle">C2–B6 · STEP 1–${n}</span></div><div class="abseq-subtle">1 Step = 1 Note / REST / TIE</div></div>
    <div class="abseq-pianoscroll" id="abseq-roll-scroll"><div id="abseq-piano">${info.svg}</div></div>
    <div id="abseq-vel" class="abseq-velscroll"></div>
    <div class="abseq-notelegend"><span style="color:#f4b75f">■ NOTE</span><span style="color:#74b8cb">■ TIE</span><span>▤ REST</span><span>Velocity-Balken unter der Piano-Roll</span></div>
   </div><aside class="abseq-card abseq-inspector"><h2>Step ${fmt(idx+1)} <span class="abseq-tag">${stepKind(step)}</span></h2><div class="abseq-notevalue">${noteName(step.note)}</div>
    <label>Tonhöhe (MIDI) <input id="abseq-note" type="number" min="0" max="127" value="${step.note}"></label>
    <label>Velocity <input id="abseq-velocity" type="number" min="1" max="127" value="${step.velocity}"></label>
    <label>Gate <input id="abseq-gate" type="number" min="1" max="127" value="${step.gate}"></label>
    <label>Swing <input id="abseq-swing" type="number" min="0" max="100" value="${step.swing}"></label>
    <label>Flags (roh) <input id="abseq-flags" type="number" min="0" max="31" value="${step.flags}"></label>
    <div class="abseq-notelegend">${[[1,'ACCENT'],[2,'SLIDE'],[4,'TIE']].map(([bit,label])=>`<label><input type="checkbox" data-flagbit="${bit}" ${step.flags&bit?'checked':''}>${label}</label>`).join('')}</div>
    <div style="display:flex;gap:7px;flex-wrap:wrap;margin:10px 0"><button id="abseq-mkrest">REST</button><button id="abseq-mktie">TIE</button><button id="abseq-duplicate">→ Duplizieren</button></div>
    <hr style="border:0;border-top:1px solid #30394a;margin:18px 0"><h2>Pattern</h2>
    <label>Länge <input id="abseq-length" type="number" min="1" max="${n}" value="${d.length}"></label>
    <label>Resolution (Gerätewert) <input id="abseq-res" type="number" min="0" max="9" value="${d.resolution}"></label>
    <div class="abseq-subtle">Gate und Resolution bleiben Gerätewerte; der Browser-Sound ist nur eine vereinfachte Hörprobe.</div></aside></div>
    <div class="abseq-bar" style="display:flex;justify-content:space-between"><span>Steps ${fmt(pageN*16+1)}–${fmt(pageN*16+16)}</span><div style="display:flex;gap:6px"><button id="abseq-prev16">◀ 16</button><button id="abseq-next16">16 ▶</button></div></div>
    <div class="abseq-stepbuttons">${cards}</div>`;
  syncScroll();bindPiano(d,info);renderVelocity(d,info);
  const setField=(id,key,lo,hi)=>{$(id).onchange=e=>{try{integer(+e.target.value,lo,hi,key);step[key]=+e.target.value;setDirty();render();}catch(err){setStatus(err.message);render();}};};
  setField('abseq-note','note',0,127);setField('abseq-velocity','velocity',1,127);setField('abseq-gate','gate',1,127);setField('abseq-swing','swing',0,100);setField('abseq-flags','flags',0,31);
  target.querySelectorAll('[data-flagbit]').forEach(el=>el.onchange=()=>{step.flags=el.checked?(step.flags|(+el.dataset.flagbit)):(step.flags&~(+el.dataset.flagbit));setDirty();render();});
  $('abseq-mkrest').onclick=()=>{step.note=0;step.flags&=~4;setDirty();render();};
  $('abseq-mktie').onclick=()=>{step.flags|=4;setDirty();render();};
  $('abseq-duplicate').onclick=()=>{if(idx<n-1){d.steps[idx+1]=clone(step);state.selectedStep=idx+1;setDirty();render();}};
  $('abseq-midi-import').onclick=()=>$('abseq-mid-file').click();
  $('abseq-mid-file').onchange=async e=>{try{const file=e.target.files[0];if(!file)return;
   const result=midiToPattern(new Uint8Array(await file.arrayBuffer()),d.bank,d.pattern);
   if(!confirm(`MIDI-Kanal ${result.channel}: ${result.filled} Noten für ${d.bank}${fmt(d.pattern)}.\n${result.discard} weitere Ereignisse entfallen (Mono/Pattern-Grenze).\n\nNur den lokalen Entwurf dieses Patterns ersetzen?`))return;
   state.history.push(clone(d));state.future=[];state.draft=result.record;state.lastDraft=clone(result.record);state.selectedStep=0;state.rollY=260;mark();render();syncHistoryButtons();
   setStatus(`MIDI importiert: ${result.filled} Noten, ${result.discard} weggelassen. Nur lokaler Browser-Entwurf.`);
  }catch(err){setStatus('MIDI-Import: '+err.message);}};
  $('abseq-midi-export').onclick=()=>{try{const bytes=patternToMidi(d),url=URL.createObjectURL(new Blob([bytes],{type:'audio/midi'})),a=document.createElement('a');a.href=url;a.download=`analogBEAST-${d.bank}${fmt(d.pattern)}.mid`;document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),60000);setStatus('MIDI des aktiven Patterns exportiert (Mono, 120 BPM).');}
   catch(err){setStatus('MIDI-Export: '+err.message);}};
  $('abseq-clear-pat').onclick=()=>{if(confirm('Alle Noten dieses Browser-Patterns leeren? Andere Patterns und Gerät bleiben unverändert.')){d.steps.forEach(s=>{s.note=0;s.flags=0;});setDirty();render();}};
  $('abseq-length').onchange=e=>{try{integer(+e.target.value,1,n,'Länge');d.length=+e.target.value;setDirty();render();}catch(err){setStatus(err.message);render();}};
  $('abseq-res').onchange=e=>{try{integer(+e.target.value,0,9,'Auflösung');d.resolution=+e.target.value;setDirty();render();}catch(err){setStatus(err.message);render();}};
  target.querySelectorAll('[data-pickstep]').forEach(el=>el.onclick=()=>{state.selectedStep=+el.dataset.pickstep;render();});
  $('abseq-prev16').disabled=pageN===0;$('abseq-next16').disabled=pageN===n/16-1;
  $('abseq-prev16').onclick=()=>{state.selectedStep=Math.max(0,(pageN-1)*16);render();};
  $('abseq-next16').onclick=()=>{state.selectedStep=Math.min(n-1,(pageN+1)*16);render();};
  $('abseq-audition').onclick=()=>{if(previewStop){stopPreview();$('abseq-audition').textContent='▶ Browser-Vorhören';}else startPreview(d);};
 }
 function renderVelocity(d,info){const n=d.steps.length,w=info.left+n*info.stepW,barY=50;
  let s=`<svg width="${w}" height="66" viewBox="0 0 ${w} 66" aria-label="Velocity pro Step"><text x="8" y="18" fill="#b9c4d3" font-size="10">VEL</text>`;
  for(let i=0;i<n;i++){const st=d.steps[i],x=info.left+i*info.stepW;
   s+=`<rect data-velstep="${i}" x="${x+3}" y="${barY-(st.velocity/127)*38}" width="${info.stepW-6}" height="${(st.velocity/127)*38}" fill="${i===state.selectedStep?'#ffda96':'#ba7734'}" opacity="${stepKind(st)==='REST'?.24:1}"/>`;
  }$('abseq-vel').innerHTML=s+'</svg>';const ve=$('abseq-vel');ve.scrollLeft=state.rollX||0;
  // Clicking a velocity column also selects the matching step; editing remains in the inspector.
  ve.querySelectorAll('[data-velstep]').forEach(el=>el.onclick=()=>{state.selectedStep=+el.dataset.velstep;render();});
 }
 async function startPreview(d){
  if(previewStop)stopPreview();
  if(!window.AudioContext&&!window.webkitAudioContext){setStatus('Browser-Audio nicht verfügbar.');return;}
  previewCtx=previewCtx||new (window.AudioContext||window.webkitAudioContext)();
  await previewCtx.resume();let i=0,stopped=false,notes=[];
  const bpm=120,ms=60000/bpm/4;
  const on=()=>{if(stopped)return;if(i>=d.length){stopPreview();const button=$('abseq-audition');if(button)button.textContent='▶ Browser-Vorhören';return;}
   if(i>=0){const s=d.steps[i];if(stepKind(s)==='NOTE'){
    const osc=previewCtx.createOscillator(),gain=previewCtx.createGain();osc.type='triangle';osc.frequency.value=440*Math.pow(2,(s.note-69)/12);
    gain.gain.value=.0;gain.gain.setTargetAtTime(.11*(s.velocity/127),previewCtx.currentTime,.005);
    gain.gain.setTargetAtTime(0,previewCtx.currentTime+Math.max(.025,(ms/1000)*(s.gate/127)),.009);
    osc.connect(gain).connect(previewCtx.destination);osc.start();osc.stop(previewCtx.currentTime+Math.max(.12,ms/1000));notes.push(osc);
   }}i++;timer=setTimeout(on,ms);};
  let timer=setTimeout(on,0);previewStop=()=>{stopped=true;clearTimeout(timer);for(const osc of notes){try{osc.stop();}catch(e){}}notes=[];};
  $('abseq-audition').textContent='■ Vorhören stoppen';
  setStatus('Browser-Vorhören bei 120 BPM mit einfachem Testton. Kein BRUTE-Audio und kein MIDI-Senden.');
 }
 function renderSong(target,d){
  state.page=Math.min(state.page,3);
  const begin=state.page*16;
  const timeline=d.rows.slice(0,d.count).map((r,i)=>`<button type="button" class="abseq-cmd" data-jump="${i}" data-type="${cmdName(r[0])}" data-chosen="${state.selectedRow===i}"><small>${fmt(i+1)} · ${cmdName(r[0])}</small><strong>${rowTitle(r)}</strong></button>`).join('');
  target.innerHTML=`<div class="abseq-titlebar"><div><h2>SONG ${fmt(d.song)} · Arrangement</h2><div class="abseq-subtle">Bis zu 64 Befehle · PATTERN / TEMPO / PRESET / REST / ARP / END</div></div>
    <div style="margin-left:auto;display:flex;gap:8px;flex-wrap:wrap"><label>Zeilen <input id="abseq-count" type="number" min="1" max="64" value="${d.count}"></label><label>Abspielen <select id="abseq-loop">${option(0,'ONCE',d.loop===0)}${option(1,'LOOP',d.loop===1)}</select></label><button id="abseq-addrow">+ Befehl</button></div></div>
   <div class="abseq-card"><h2>Timeline</h2><div class="abseq-cmdstrip">${timeline}</div><p class="abseq-subtle">Anklicken, um eine Zeile zu bearbeiten. END stoppt den Song. Die Reihenfolge entspricht dem FM-1.</p></div>
   <div class="abseq-bar"><strong>Befehle ${fmt(begin+1)}–${fmt(begin+16)}</strong><div style="margin-left:auto;display:flex;align-items:center;gap:6px"><button id="abseq-prev">◀</button> Seite ${state.page+1} / 4 <button id="abseq-next">▶</button></div></div>
   <div class="abseq-songrows">${d.rows.slice(begin,begin+16).map((r,j)=>{const i=begin+j,t=r[0],isPattern=t<8,kind=cmdName(t),enabled=i<d.count;
    return `<div class="abseq-songrow" data-active="${enabled}" style="${!enabled?'opacity:.52;':''}"><div class="abseq-rowhead"><strong>${fmt(i+1)} · ${kind}</strong><span class="abseq-subtle">${rowTitle(r)}</span></div>
      <div class="abseq-rowfields"><label>Typ <select data-row="${i}" data-col="kind">${['PATTERN','END','TEMPO','PRESET','REST','ARP'].map((key,j)=>option(key,key,key===kind)).join('')}</select></label>
      ${isPattern?`<label>Bank <select data-row="${i}" data-col="bank">${[...BANKS].map((bank,ix)=>option(ix,bank,ix===t)).join('')}</select></label><label>Pattern <input type="number" min="1" max="16" value="${r[1]+1}" data-row="${i}" data-col="pattern"></label><label>Steps <input type="number" min="1" max="${t<4?64:16}" value="${r[2]}" data-row="${i}" data-col="steps"></label>`:
      kind==='REST'?`<label>Steps <input type="number" min="1" max="64" value="${r[2]}" data-row="${i}" data-col="steps"></label>`:
      kind==='END'?'':`<label>${kind==='TEMPO'?'BPM':kind==='PRESET'?'Preset (0–31)':'ARP 0/1'} <input type="number" min="${kind==='TEMPO'?40:0}" max="${kind==='TEMPO'?240:kind==='PRESET'?31:1}" value="${r[3]}" data-row="${i}" data-col="value"></label>`}
      <button class="abseq-smallbtn" data-up="${i}" title="Befehl nach oben">↑</button><button class="abseq-smallbtn" data-down="${i}" title="Befehl nach unten">↓</button>
      </div></div>`;}).join('')}</div>`;
  $('abseq-count').onchange=e=>{try{integer(+e.target.value,1,64,'SONG-Länge');d.count=+e.target.value;setDirty();render();}catch(err){setStatus(err.message);render();}};
  $('abseq-loop').onchange=e=>{d.loop=+e.target.value;setDirty();};
  $('abseq-addrow').onclick=()=>{if(d.count===64){setStatus('Song hat bereits 64 Zeilen.');return;}d.rows[d.count]=[8,16,1,0];d.count++;state.page=Math.floor((d.count-1)/16);setDirty();render();};
  $('abseq-prev').onclick=()=>{state.page=(state.page+3)%4;render();};$('abseq-next').onclick=()=>{state.page=(state.page+1)%4;render();};
  target.querySelectorAll('[data-jump]').forEach(b=>b.onclick=()=>{state.selectedRow=+b.dataset.jump;state.page=Math.floor(state.selectedRow/16);render();});
  target.querySelectorAll('[data-row]').forEach(el=>el.onchange=()=>{const i=+el.dataset.row,r=d.rows[i];try{
   const v=el.value,c=el.dataset.col;
   if(c==='kind')d.rows[i]=v==='PATTERN'?[0,0,16,0]:v==='END'?[8,16,1,0]:v==='TEMPO'?[9,0,1,120]:v==='PRESET'?[10,0,1,0]:v==='REST'?[11,0,16,0]:[12,0,1,1];
   else if(c==='bank'){r[0]=+v;if(r[0]>=4)r[2]=Math.min(16,r[2]);}
   else if(c==='pattern'){integer(+v,1,16,'Pattern');r[1]=+v-1;}
   else if(c==='steps'){integer(+v,1,r[0]>=4&&r[0]<8?16:64,'Steps');r[2]=+v;}
   else if(c==='value'){const lo=r[0]===9?40:0,hi=r[0]===9?240:r[0]===10?31:1;integer(+v,lo,hi,'Wert');r[3]=+v;}
   validateRow(d.rows[i]);state.selectedRow=i;setDirty();render();
  }catch(err){setStatus(err.message);render();}});
  target.querySelectorAll('[data-up],[data-down]').forEach(el=>el.onclick=()=>{const i=+(el.dataset.up??el.dataset.down),to=el.hasAttribute('data-up')?i-1:i+1;if(to<0||to>=d.count)return;[d.rows[i],d.rows[to]]=[d.rows[to],d.rows[i]];state.selectedRow=to;state.page=Math.floor(to/16);setDirty();render();});
 }
 function renderAlbum(target,d){
  target.innerHTML=`<div class="abseq-titlebar"><div><h2>ALBUM ${d.album} · Playlist</h2><div class="abseq-subtle">Bis zu 32 Songs, in der angezeigten Reihenfolge. Der FM-1 spielt Songs bis END.</div></div>
   <div style="margin-left:auto;display:flex;gap:7px;flex-wrap:wrap;align-items:center"><label>Slots <input id="abseq-count" type="number" min="0" max="32" value="${d.count}"></label><label>Playback <select id="abseq-loop">${option(0,'ONCE',d.loop===0)}${option(1,'LOOP',d.loop===1)}</select></label><button id="abseq-addslot">+ SONG anhängen</button></div></div>
   <div class="abseq-card"><h2>Song-Reihenfolge</h2><p class="abseq-subtle">Nummern auswählen oder mit ↑ ↓ verschieben. Klick auf Bearbeiten öffnet SONG EDIT (ungespeicherte Album-Änderungen vorher exportieren).</p>
    <div class="abseq-albumgrid">${d.song_ids.map((id,i)=>`<div class="abseq-albumslot" data-empty="${i>=d.count}"><strong>${fmt(i+1)}</strong>${i<d.count?`<select data-slot="${i}">${options(32,id||1,'SONG ')}</select><button class="abseq-sm-btn" data-move="${i}" data-dir="-1" title="nach oben">↑</button><button class="abseq-sm-btn" data-move="${i}" data-dir="1" title="nach unten">↓</button>`:'<span class="abseq-subtle">— leer —</span>'}</div>`).join('')}</div></div>`;
  $('abseq-count').onchange=e=>{try{integer(+e.target.value,0,32,'Slots');const count=+e.target.value;d.song_ids=d.song_ids.map((v,i)=>i<count?(v===null?1:v):null);d.count=count;setDirty();render();}catch(err){setStatus(err.message);render();}};
  $('abseq-addslot').onclick=()=>{if(d.count===32){setStatus('Album ist voll (32 Songs).');return;}d.song_ids[d.count]=1;d.count++;setDirty();render();};
  $('abseq-loop').onchange=e=>{d.loop=+e.target.value;setDirty();};
  target.querySelectorAll('[data-slot]').forEach(el=>el.onchange=()=>{d.song_ids[+el.dataset.slot]=+el.value;setDirty();render();});
  target.querySelectorAll('[data-move]').forEach(el=>el.onclick=()=>{const i=+el.dataset.move,to=i+(+el.dataset.dir);if(to<0||to>=d.count)return;[d.song_ids[i],d.song_ids[to]]=[d.song_ids[to],d.song_ids[i]];setDirty();render();});
 }
 async function busyAction(fn){if(state.busy)return;state.busy=true;for(const el of panel.querySelectorAll('button'))el.disabled=true;
  try{await fn();}catch(e){console.error('analogBEAST SEQ:',e);setStatus('FEHLER: '+e.message);}
  finally{state.busy=false;for(const el of panel.querySelectorAll('button'))el.disabled=false;syncHistoryButtons();}}
 function mustMIDI(){if(!state.proto)throw Error('Zuerst MIDI verbinden');return state.proto;}
 function download(name,obj){const blob=new Blob([JSON.stringify(obj,null,2)+'\n'],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=name;document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),60000);}
 function timeName(){return new Date().toISOString().replace(/[:.]/g,'-');}
 async function connect(){
  if(!window.isSecureContext)throw Error('WebMIDI braucht HTTPS oder localhost. Nicht file:// öffnen.');
  if(!navigator.requestMIDIAccess)throw Error('WebMIDI ist nicht verfügbar – Chrome/Edge und MIDI-Berechtigung prüfen.');
  setStatus('Frage WebMIDI-Berechtigung für SysEx an …');
  try{state.access=await navigator.requestMIDIAccess({sysex:true});}
  catch(e){throw Error(`WebMIDI-Zugriff/SysEx verweigert (${e.name||e.message}). Browser-Site-Berechtigungen und ggf. Unternehmensrichtlinien prüfen.`);}
  const refresh=async()=>{
   const ins=[...state.access.inputs.values()].filter(p=>p.state==='connected'),outs=[...state.access.outputs.values()].filter(p=>p.state==='connected');
   const prevIn=$('abseq-in').value,prevOut=$('abseq-out').value;
   for(const [id,ports,selected] of [['abseq-in',ins,prevIn],['abseq-out',outs,prevOut]]){
    const el=$(id);el.innerHTML=ports.length?ports.map(p=>option(p.id,p.name||p.id)).join(''):'<option value="">— kein Port erkannt —</option>';
    if(ports.some(p=>p.id===selected))el.value=selected;
    else {const preferred=ports.filter(p=>/analogbeast|fm-1|felucca/i.test(p.name||''));if(preferred.length)el.value=preferred[0].id;}
   }
   const change=async()=>{const inp=state.access.inputs.get($('abseq-in').value),out=state.access.outputs.get($('abseq-out').value);
    state.proto=null;if(!inp||!out){setStatus(`WebMIDI verfügbar, aber Ports unvollständig (IN ${ins.length}, OUT ${outs.length}). USB-Verbindung, Kabel, Browser-Berechtigung und andere MIDI-Programme prüfen. Danach „MIDI verbinden / Aktualisieren“ drücken.`);return;}
    try{await inp.open();await out.open();state.proto=new Protocol(out,inp);setStatus(`MIDI bereit: IN=${inp.name} | OUT=${out.name}. USB SysEx 96 · Für Lesen Gerät auf STOP und REC verlassen.`);}
    catch(e){setStatus('MIDI-Port konnte nicht geöffnet werden: '+e.message+'. Andere DAWs/Tools schließen und erneut versuchen.');}
   };
   $('abseq-in').onchange=()=>{change().catch(e=>setStatus('MIDI: '+e.message));};
   $('abseq-out').onchange=()=>{change().catch(e=>setStatus('MIDI: '+e.message));};
   await change();
  };
  state.access.onstatechange=()=>{refresh().catch(e=>setStatus('MIDI-Gerätewechsel: '+e.message));};
  await refresh();
 }
 $('abseq-connect').onclick=()=>busyAction(connect);
 $('abseq-read').onclick=()=>busyAction(async()=>{if(!allowDiscard())return;const p=mustMIDI();const d=await p.read(state.mode,keyFor());state.draft=clone(d);state.draftType=state.mode;state.dirty=false;state.entries=null;state.page=0;state.history=[];state.future=[];state.lastDraft=clone(state.draft);render();setStatus('Vom Gerät gelesen: '+keyOf(state.mode,d)+' (nur Browser, nicht verändert).');});
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
   if(parsed.length===1){const item=parsed[0],r=item.record;state.mode=item.type;state.draft=clone(r);state.draftType=item.type;state.entries=null;state.history=[];state.future=[];state.lastDraft=clone(state.draft);
    Object.assign(state.key,item.type==='pattern'?{bank:r.bank,pattern:r.pattern}:item.type==='song'?{song:r.song}:{album:r.album});updatePanel();setStatus('Import-Vorschau: '+keyOf(item.type,r)+' (noch nicht ans Gerät gesendet).');}
   else {state.entries=parsed;state.draft=null;state.history=[];state.future=[];state.lastDraft=null;updatePanel();setStatus(`Import-Vorschau: ${parsed.length} Datensätze. Vor dem Schreiben werden ALLE Ziele gesichert.`);}
  }catch(err){setStatus('JSON-Importfehler: '+err.message);}};
 $('abseq-write').onclick=()=>busyAction(async()=>{
  const items=state.entries?[...state.entries]:(loaded()?[{type:state.mode,record:clone(state.draft)}]:null);
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
  await p.waitSaved(items,setStatus);state.dirty=false;state.entries=null;state.history=[];state.future=[];state.lastDraft=state.draft?clone(state.draft):null;
  setStatus(`PASS: ${items.length} Ziel(e) geschrieben, vollständig zurückgelesen, Autosave bestätigt. Backup: ${name}`);
 });
 $('abseq-undo').onclick=()=>useHistory('undo');$('abseq-redo').onclick=()=>useHistory('redo');
 document.addEventListener('keydown',e=>{if(!panel.classList.contains('abseq-visible')||state.busy||!e.ctrlKey||e.altKey)return;
  if(['INPUT','TEXTAREA','SELECT'].includes(document.activeElement?.tagName))return;
  if(e.key.toLowerCase()==='z'){e.preventDefault();useHistory(e.shiftKey?'redo':'undo');}
  if(e.key.toLowerCase()==='y'){e.preventDefault();useHistory('redo');}
 });
 panel.querySelectorAll('.abseq-tab').forEach(btn=>btn.onclick=()=>{if(btn.dataset.mode===state.mode)return;if(!allowDiscard())return;state.mode=btn.dataset.mode;clearDraft();updatePanel();});
 open.onclick=()=>{panel.classList.add('abseq-visible');open.hidden=true;};
 $('abseq-close').onclick=()=>{panel.classList.remove('abseq-visible');open.hidden=false;};
 updatePanel();
 if(window.location.hash==='#sequencer')open.click();
 window.addEventListener('hashchange',()=>{if(window.location.hash==='#sequencer')open.click();});
}
if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',init,{once:true});else init();
})(typeof window!=='undefined'?window:globalThis);
