'use strict';
const assert=require('node:assert/strict');
const mod=require('../tools/ab_seq_payload/analogbeast-seq.js');
const mkPat=(bank='A',pattern=1)=>({bank,pattern,length:bank<='D'?64:16,resolution:2,steps:Array.from({length:bank<='D'?64:16},()=>mod.DEFAULT_STEP())});
const mkSong=()=>({song:2,count:9,loop:0,rows:Array.from({length:64},()=>[8,16,1,0])});
const mkAlbum=()=>({album:1,count:2,loop:1,song_ids:[1,2,...Array(30).fill(null)]});
function testValidation(){
 for(const b of mod.BANKS)assert.equal(mod.checkRecord('pattern',mkPat(b)).bank,b);
 assert.equal(mod.checkRecord('song',mkSong()).count,9);
 assert.equal(mod.checkRecord('album',mkAlbum()).count,2);
 const p=mkPat('E');p.steps.push(mod.DEFAULT_STEP());assert.throws(()=>mod.checkRecord('pattern',p));
 const s=mkSong();s.rows[0]=[4,1,17,0];assert.throws(()=>mod.checkRecord('song',s));
 assert.equal(mod.parseDocument(mod.docOf([{type:'pattern',record:mkPat()}, {type:'song',record:mkSong()}, {type:'album',record:mkAlbum()}])).length,3);
 assert.throws(()=>mod.parseDocument(mod.docOf([{type:'song',record:mkSong()},{type:'song',record:mkSong()}])));
}
class In {
 constructor(){this.events=new Set();}
 addEventListener(type,fn){this.events.add(fn);}
 removeEventListener(type,fn){this.events.delete(fn);}
 emit(data){for(const fn of [...this.events])fn({data});}
}
function emit(out,body){return [240,...mod.MAGIC,...body,247];}
function fakeDevice(){
 const input=new In(),storage={A1:mkPat('A'), E2:mkPat('E',2),song2:mkSong(),album1:mkAlbum()};
 const key=(bank,pattern)=>String.fromCharCode(65+bank)+(pattern+1);
 const send={send(a){const op=a[5],token=a[6],d=a.slice(7,-1);let r=[];
  if(op===17){const o=storage[key(d[0],d[1])];r=[d[0],d[1],o.length,o.resolution];}
  if(op===15){const o=storage[key(d[0],d[1])];r=[...d,...o.steps.slice(d[2],d[2]+d[3]).flatMap(s=>mod.FIELDS.map(f=>s[f]))];}
  if(op===14){const o=storage[key(d[0],d[1])];for(let i=0;i<d[3];i++)o.steps[d[2]+i]=Object.fromEntries(mod.FIELDS.map((f,j)=>[f,d[4+i*5+j]]));r=d.slice(0,4);}
  if(op===16){const o=storage[key(d[0],d[1])];o.length=d[2];o.resolution=d[3];r=d;}
  if(op===22){const o=storage[key(d[0],d[1])];r=[d[0],d[1],o.length,o.resolution,...o.steps.flatMap(s=>mod.FIELDS.map(f=>s[f]))];}
  if(op===21){const o=storage[key(d[0],d[1])];o.length=d[2];o.resolution=d[3];o.steps=Array.from({length:16},(_,i)=>Object.fromEntries(mod.FIELDS.map((f,j)=>[f,d[4+i*5+j]])));r=d.slice(0,2);}
  if(op===28){const o=storage['song'+(d[0]+1)];r=[d[0],o.count,o.loop];}
  if(op===24){const o=storage['song'+(d[0]+1)];r=[...d,...o.rows.slice(d[1],d[1]+d[2]).flatMap(([b,p,s,v])=>[b,p,s,v&127,v>>7])];}
  if(op===23){const o=storage['song'+(d[0]+1)];for(let i=0;i<d[2];i++){let x=d.slice(3+5*i,8+5*i);o.rows[d[1]+i]=[x[0],x[1],x[2],x[3]|(x[4]<<7)];}r=d.slice(0,3);}
  if(op===27){const o=storage['song'+(d[0]+1)];o.count=d[1];o.loop=d[2];r=d;}
  if(op===26){const o=storage['album'+(d[0]+1)];r=[d[0],o.count,o.loop,...o.song_ids.map(id=>id===null?127:id-1)];}
  if(op===25){storage['album'+(d[0]+1)]={album:d[0]+1,count:d[1],loop:d[2],song_ids:d.slice(3).map(v=>v===127?null:v+1)};r=d.slice(0,3);}
  if(op===6)r=[0,0];if(op===30)r=[0,0,1,1,1];
  queueMicrotask(()=>input.emit(emit(null,[op,token,0,...r])));
 }};return {input,send,storage};
}
async function protocol(){const d=fakeDevice(),p=new mod.Protocol(d.send,d.input);
 for(const [type,key] of [['pattern',{bank:'A',pattern:1}],['pattern',{bank:'E',pattern:2}],['song',{song:2}],['album',{album:1}]]){
  const old=await p.read(type,key);const updated=structuredClone(old);
  if(type==='pattern')updated.steps[0].note=77;
  if(type==='song'){updated.rows[0]=[9,0,1,200];updated.count=8;}
  if(type==='album')updated.song_ids[0]=2;
  await p.write(type,updated);assert.deepEqual(await p.read(type,key),updated);await p.waitSaved([{type,record:updated}]);
 }
 assert.equal(d.input.events.size,0,'MIDI listeners cleaned up');
}
(async()=>{testValidation();await protocol();console.log('PASS: A-D 64-step, E-H 16-step, 32-song wire, albums, validation, backup-compatible JSON, readback and autosave');})().catch(err=>{console.error(err);process.exit(1)});
