'use strict';
// Standalone fixtures, not user songs. Run: node tests/test_midi_song.js
const assert=require('assert');const seq=require('../tools/ab_midi_song_payload/analogbeast-seq.js');
function vq(n){let q=[n&127];for(n>>=7;n;n>>=7)q.unshift((n&127)|128);return q;}
function u32(n){return [(n>>>24)&255,(n>>>16)&255,(n>>>8)&255,n&255];}
function track(events){events.sort((a,b)=>a.tick-b.tick);const out=[];let last=0;for(const e of events){out.push(...vq(e.tick-last),...e.data);last=e.tick;}out.push(0,255,47,0);return [77,84,114,107,...u32(out.length),...out];}
function midi({notes=36,bpm=108,n=3,d=4,overlap=false,change=false}={}){
 const micro=Math.round(60000000/bpm);const meta=track([{tick:0,data:[255,81,3,(micro>>16)&255,(micro>>8)&255,micro&255]},{tick:0,data:[255,88,4,n,Math.log2(d),24,8]},...(change?[{tick:384*8,data:[255,81,3,8,186,44]}]:[])]);
 const ev=[];for(let i=0;i<notes;i++){const tick=i*192;ev.push({tick,data:[0x90,60+i%12,100]},{tick:tick+(overlap?250:160),data:[0x80,60+i%12,0]});}
 const music=track(ev);return Uint8Array.from([77,84,104,100,0,0,0,6,0,1,0,2,1,128,...meta,...music]);
}
for(const [count,bpm,meter,expectedPatterns] of [[36,110,'3/4',2],[80,116,'4/4',5],[61,108,'2/4',4],[30,76,'2/4',2]]){
 const [n,d]=meter.split('/').map(Number),parsed=seq.midiSongParse(midi({notes:count,bpm,n,d}));const group=seq.midiSongGroups(parsed)[0];
 assert.equal(group.count,count);const plan=seq.midiSongPlan(parsed,{track:1,channel:0,startBank:'A',startPattern:9,song:3,preset:1,barSplit:false});
 assert.equal(plan.totalNotes,count);assert.equal(plan.firstBpm,bpm);assert.equal(plan.meter,meter);
 assert.equal(plan.song.rows[0][3],bpm);assert.deepEqual(plan.song.rows[1],[10,0,1,0]);
 assert.deepEqual(plan.song.rows[plan.song.count-1],[8,16,1,0]);
 assert.equal(seq.parseDocument(seq.docOf(plan.entries)).length,plan.patterns.length+1);
 assert.equal(plan.patterns.reduce((v,p)=>v+p.steps.filter(x=>x.note>0).length,0),count);
}
const p=seq.midiSongParse(midi({notes:80,n:3,d:4}));
const aligned=seq.midiSongPlan(p,{track:1,channel:0,startBank:'A',startPattern:1,song:1,barSplit:true});
assert(aligned.patterns.slice(0,-1).every(p=>p.length===60));
assert.throws(()=>seq.midiSongPlan(p,{track:1,channel:0,startBank:'H',startPattern:16,song:1}),/Nicht genug/);
const rec=seq.midiSongPlan(p,{track:1,channel:0,startBank:'E',startPattern:1,song:1});
assert(rec.patterns.every(x=>x.steps.length===16&&x.length<=16));
const overlap=seq.midiSongParse(midi({notes:10,overlap:true}));
assert.throws(()=>seq.midiSongPlan(overlap,{track:1,channel:0,startBank:'A',startPattern:1,song:1}),/Überlappende/);
const changed=seq.midiSongParse(midi({notes:40,change:true}));const cp=seq.midiSongPlan(changed,{track:1,channel:0,startBank:'A',startPattern:1,song:1});
assert(cp.rows.filter(x=>x[0]===9).length===2);
console.log('PASS: whole-song MIDI SMF0/1, note preservation, P01, TEMPO/END, 16/64 step, overflow, overlap, MIDI tempo changes');
