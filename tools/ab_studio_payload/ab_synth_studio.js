/* analogBEAST Studio v1.0: firmware-aware Sound tab for original Felucca editor.
 * Keep the original editor's WebMIDI Link, params, presets, project, samples,
 * settings and licensing. Inject inside its IIFE, before buildGroups().
 * GPL-3.0-only; see root LICENSE.
 */
function abStudioSupported(info) {
  // All mapped IDs come from the analogBEAST v1.0 firmware/src/core.h
  // GOLANA GL_* aliases; pe0-7 ... pe0-1 are the seven added BRUTE params.
  return !!(info && dev && dev.dump && Array.isArray(info.engines) &&
    String(info.engines[dev.dump.engine] || '').toUpperCase() === 'GOLANA' &&
    Number.isInteger(info.pe0) && info.pe0 >= 110 &&
    info.pcount === info.pe0 + 8 && info.gcount >= 27 &&
    dev.pdesc && dev.gdesc && dev.dump.p && dev.dump.g);
}
function abStudioBuild(groups, info) {
  const E=info.pe0, used=new Set(), usedG=new Set();
  const LABELS={
    [E-7]:['SYNTH','MIKROKORG / BRUTE engine selection'],
    [E-6]:['BFACT','Brute Factor · filter feedback and saturation'],
    [E-5]:['SP MODE','Steiner-Parker LP / BP / HP'],
    [E-4]:['ULTRA','Ultrasaw detuning amount'],
    [E-3]:['PWM','Pulse width modulation'],
    [E-2]:['METAL','Triangle Metalizer'],
    [E-1]:['SUB','Sub / overtone mix'],
    [E]:['WAVE 1','Oscillator 1 wave'],
    [E+3]:['WAVE 2','Oscillator 2 wave'],
    [E+7]:['CUTOFF','GOLANA filter frequency']
  };
  const pretty=(row,id)=>{
    const meta=LABELS[id];if(!meta)return;
    const lab=row.firstElementChild;if(lab)lab.textContent=meta[0];
    row.title=meta[1];
    const ctlInput=row.querySelector('input,select');if(ctlInput)ctlInput.setAttribute('aria-label',meta[1]);
  };
  const add=(group,title,sub,ids,tag) => {
    const box=el('div',{class:'group ab-studio-panel '+(tag||'')},
      el('h2',{text:title}),el('p',{class:'ab-subtle',text:sub}));
    let n=0;
    for(const id of ids){
      if(!Number.isInteger(id)||id<0||id>=info.pcount||used.has(id))continue;
      const row=paramRow(0,id);
      if(!row)continue;
      pretty(row,id); used.add(id); box.append(row);n++;
    }
    if(n)groups[group].push(box);
  };
  const globals=(title,sub,ids) => {
    const box=el('div',{class:'group ab-studio-panel'},el('h2',{text:title}),el('p',{class:'ab-subtle',text:sub}));
    let n=0;
    for(const id of ids){
      if(!Number.isInteger(id)||id<0||id>=info.gcount||usedG.has(id))continue;
      const row=paramRow(1,id);
      if(row){usedG.add(id);box.append(row);n++;}
    }
    if(n)groups.setgroups.push(box);
  };
  // This is intentionally descriptor-backed. Rendering and setting values goes
  // through the original paramRow/userSet/Link machinery, NOT an invented SysEx.
  add('groups','01 · SOUND MODE','MIKROKORG oder BRUTE · Klangcharakter und Steiner-Parker Filter',
    [E-7,E-6,E-5,E-4,E-3,E-2,E-1],'ab-brute');
  add('groups','02 · OSCILLATOR 1','Wellenform · CTRL 1/2 · Lautstärke · Wavefolder',
    [E,E+1,E+2,61,101],'ab-osc');
  add('groups','03 · OSCILLATOR 2','Wellenform · Ring/Sync · Tonhöhe · Mischung',
    [E+3,E+4,E+5,E+6,62],'ab-osc');
  add('groups','04 · MIXER & FILTER','Noise · Cutoff · LP24/LP12/BP/HP/MS20 · Resonanz · Filter EG',
    [63,E+7,64,65,66,67],'ab-filter');
  add('groups','05 · FILTER ENVELOPE','Eigenständige Filterhüllkurve · Distortion · Retrigger',
    [68,69,70,71,72,73],'ab-filter');
  add('groups','06 · AMPLIFIER ENVELOPE','ADSR · synchronisierte Zeiten',
    [0,1,2,3,4,102],'ab-env');
  add('groups','07 · LFO 1','Wellenform, Geschwindigkeit und Sync',
    [74,75,76,77],'ab-mod');
  add('groups','08 · LFO 2','Wellenform, Geschwindigkeit und Sync',
    [78,79,80,83],'ab-mod');
  add('groups','09 · MOD PATCHES','Vier routbare Modulationsverbindungen',
    [49,50,51,52,53,54,55,56,57,58,59,60],'ab-mod');
  add('groups','10 · ARPEGGIATOR','Golana ARP · Last/Stepmask sowie globale ARP-Einstellungen',
    [84,85,86,87,88,89,17,18,19,20,21,22,23,24],'ab-arp');
  add('groups','11 · DELAY / REVERB / MFX','Golana-Effektweg · Effektreihenfolge',
    [90,8,9,10,11,12,92,93,33,34,35,36],'ab-fx');
  add('groups','12 · VOICES & PERFORMANCE','Voice mode · Glide · Pan · Detune · Skalen',
    [37,38,39,40,41,42,43,44,95,25,26,27,28],'ab-voice');
  add('groups','13 · INSERT / SLICER','Weitere Effekte und der Slicer',
    [45,46,47,48,96,97,98,99,100],'ab-fx');
  add('seqgroups','TRACK STEP SETTINGS','Parameter des bestehenden Track-Step-Sequencers',
    [29,30,31,32,94],'ab-arp');
  // Never silently hide an exposed firmware parameter. Some internal legacy
  // params are reused for GOLANA; they remain available in an advanced card.
  const remaining=[];
  for(let i=0;i<info.pcount;i++)if(!used.has(i))remaining.push(i);
  add('groups','ADVANCED · weitere Parameter','Direkt aus den Descriptoren der Firmware; nicht in einer Kategorie oben',remaining,'ab-advanced');
  globals('CLOCK & MIDI','Tempo · Swing · Routing · Tuning',[0,1,2,3,12,13,14,15]);
  globals('FX & SYSTEM','Gemeinsame Effekte und globale Parameter',Array.from({length:info.gcount},(_,i)=>i));
  const pill=document.getElementById('abstudio-device');
  if(pill)pill.textContent=`analogBEAST · ${info.pcount} Sound-Parameter · ${info.ntrk || 1} Tracks`;
  const mode=document.getElementById('abstudio-mode');
  if(mode)mode.textContent='GOLANA · BRUTE / MIKROKORG';
}
