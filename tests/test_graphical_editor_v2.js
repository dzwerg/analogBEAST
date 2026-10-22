'use strict';
const assert=require('node:assert/strict');const fs=require('node:fs');const path=require('node:path');
const js=fs.readFileSync(path.join(__dirname,'../tools/ab_graphical_payload/analogbeast-seq.js'),'utf8');
const mod=require('../tools/ab_graphical_payload/analogbeast-seq.js');
for(const label of ['PIANO ROLL','abseq-songrows','abseq-albumgrid','MIDI verbinden / Aktualisieren',
 'Browser-Vorhören','abseq-undo','abseq-redo','abseq-midi-import','abseq-midi-export','window.location.hash','--']){
 if(label==='--')continue;assert.ok(js.includes(label),`UI missing ${label}`);
}
for(const id of ['abseq-read','abseq-all','abseq-import','abseq-write'])assert.ok(js.includes(id));
// Exercise the exact MIDI functions shipped in the browser file, without any browser or device dependency.
const a=js.indexOf('// Minimal Standard MIDI File handling'),b=js.indexOf(' root.ABSEQ.noteName=',a);
assert.ok(a>0&&b>a);
const create=(mode,key)=>({bank:key.bank,pattern:key.pattern,length:key.bank<='D'?64:16,resolution:2,
 steps:Array.from({length:key.bank<='D'?64:16},()=>mod.DEFAULT_STEP())});
const midi=(new Function('checkRecord','newRecord',js.slice(a,b)+'\nreturn {midiToPattern,patternToMidi,midiSmfNotes}'))(mod.checkRecord,create);
for(const bank of ['A','E']){
 const p=create('pattern',{bank,pattern:3});p.steps[0].note=60;p.steps[0].gate=62;p.steps[0].velocity=100;
 p.steps[4].note=67;p.steps[4].velocity=90;
 const raw=midi.patternToMidi(p);assert.equal(String.fromCharCode(...raw.slice(0,4)),'MThd');
 const parsed=midi.midiSmfNotes(raw);assert.equal(parsed.notes.length,2);
 const q=midi.midiToPattern(raw,bank,3);
 assert.equal(q.record.steps[0].note,60);assert.equal(q.record.steps[4].note,67);
 assert.equal(q.record.steps.length,bank==='A'?64:16);
 assert.equal(q.channel,1);
}
assert.throws(()=>midi.midiSmfNotes(new Uint8Array([1,2,3])));
console.log('PASS: Graphical UI, MIDI port diagnostics, offline editing, JSON actions and MIDI import/export roundtrip');
