#!/usr/bin/env python3
"""analogBEAST v1.0 real GOLANA program chooser. Web editor only.

Shows the 32 true GOLANA programs, not Felucca engines/presets.
Based on firmware/src/golana_prog.c, golana_ui.c, golana_factory.c,
core.h, midi_control.c. Does not write to a device from Python.
"""
from pathlib import Path
import argparse, datetime, html, re, shutil, sys

ROOT=Path(__file__).resolve().parent.parent
TARGET=ROOT/'web'/'editor.html'
BACKUP=ROOT/'web'/'editor.html.before-ab-instrument-v2'
MARK='<!-- AB_INSTRUMENT_P01_P32_V2 -->'
FACTORY=('FAT BASS','ACID LINE','GLIDE LEAD','UNISON SAW','SYNC PAD','BRIGHT ARP','80S BRASS','ORGAN','SOFT PAD','PHASE PAD','SLOW ARP','SOFT BELLS','DRONE','WARM LEAD','DARK UNISON','S&H TEXTURE')
STYLE='''<style id="ab-instruments-v2-css">
/* All original Felucca controls stay in the DOM for existing code, but are not GOLANA programs. */
#p-sound #browser > label { display:none !important; }
#p-sound #ab-instruments { box-sizing:border-box; display:flex !important; flex-flow:row wrap; align-items:end;
 gap:10px 16px; padding:12px 14px; border:1px solid #684620; border-radius:9px;
 background:#1c2028; width:100%; margin-bottom:8px; }
#p-sound #ab-instruments label { display:flex; flex-direction:column; gap:5px;
 color:#f7aa53; font-weight:700; flex:1 1 230px; }
#p-sound #ab-instrument-program {width:100%; max-width:530px; min-width:200px;
 background:#131922; color:#f2f4f8; border:1px solid #687182; border-radius:6px;
 padding:10px; font:inherit; }
#p-sound #ab-instrument-refresh {background:#303a48; color:#fff; border:1px solid #596475;
 border-radius:6px; padding:10px 12px; }
#p-sound #ab-instrument-feedback { flex-basis:100%; color:#bac4d0; font-size:12px; }
</style>'''
OPTIONS='\n'.join('<option value="%d">P%02d · %s</option>'%(i,i+1,html.escape(FACTORY[i] if i<16 else 'INIT')) for i in range(32))
PANEL='''<div id="ab-instruments" data-firmware="GOLANA-32">
<label for="ab-instrument-program">INSTRUMENT · P01–P32
<select id="ab-instrument-program" disabled>
<option value="">— Gerät verbinden —</option>
%s
</select></label>
<button type="button" id="ab-instrument-refresh">Programmnamen lesen</button>
<div id="ab-instrument-feedback" role="status">P01–P16: Werkklänge. P17–P32: INIT oder gespeicherte Sounds. Gerät verbinden.</div>
</div>'''%OPTIONS
JS=r'''
/* analogBEAST instrument selection v2. Firmware GOLANA: 32 program slots, PC 0..31. */
const AB_INST_FACTORY = Object.freeze(["FAT BASS","ACID LINE","GLIDE LEAD","UNISON SAW","SYNC PAD","BRIGHT ARP","80S BRASS","ORGAN","SOFT PAD","PHASE PAD","SLOW ARP","SOFT BELLS","DRONE","WARM LEAD","DARK UNISON","S&H TEXTURE"]);
let abInstSelected = "", abInstDevice = null, abInstBusy = false;
function abInstElements() {
  return {sel:document.getElementById('ab-instrument-program'),
    refresh:document.getElementById('ab-instrument-refresh'),
    feedback:document.getElementById('ab-instrument-feedback')};
}
function abInstStatus(s) { const e=abInstElements().feedback;if(e)e.textContent=s; }
function abInstIsGolana(d) {return !!(d&&d.info&&d.dump&&d.output&&d.info.engines&&d.info.engines[13]==='GOLANA');}
function abInstRender() {
  const {sel,refresh}=abInstElements(); if(!sel)return;
  const d=abInstIsGolana(dev)?dev:null;
  if (d!==abInstDevice) {abInstDevice=d;abInstSelected='';}
  sel.disabled=!d; refresh.disabled=!d||abInstBusy;
  const slots=Array.isArray(d?.bank?.slots)?d.bank.slots:null;
  const names=[];
  for(let i=0;i<32;i++){
    const record=slots?.find(x=>Number(x.slot)===i);
    const owned=!!(record?.used && Number(record.engine)===13);
    const name=owned?String(record.name||'Ohne Namen'):(i<16?AB_INST_FACTORY[i]:'INIT');
    names.push(`P${String(i+1).padStart(2,'0')} · ${name}${owned?' [Gespeichert]':''}`);
  }
  const old=sel.dataset.signature||'', signature=names.join('\n');
  if(signature!==old){
    // Keep an explicit unknown state: DUMP cannot report the currently selected gu.prog.
    sel.replaceChildren(new Option('— Programm wählen —',''),...names.map((n,i)=>new Option(n,String(i))));
    sel.dataset.signature=signature;
  }
  sel.value=abInstSelected;
  if(!d)abInstStatus('Gerät verbinden. Nur GOLANA-Programme P01–P32, keine Felucca-Engines.');
  else if(!slots)abInstStatus('GOLANA verbunden. Programmnamen vom FM-1 noch nicht gelesen.');
  else if(!abInstSelected)abInstStatus('32 Programme bereit. Aktuelles Hardwareprogramm ist nicht per Felucca-DUMP rücklesbar.');
}
async function abInstReadNames() {
  if(!abInstIsGolana(dev)||abInstBusy)return;
  const d=dev; abInstBusy=true; abInstRender();
  abInstStatus('Lese 32 Programmplätze …');
  try {
    d.bank=await bank.list((r,o)=>sessionRequest(d,r,{...o,quiet:true}));
    if(dev!==d)return;
    abInstRender();abInstStatus('Namen von den 32 Geräteslots gelesen.');
  }catch(e){abInstStatus('Lesen fehlgeschlagen: '+e.message);}
  finally{abInstBusy=false;abInstRender();}
}
async function abInstChoose() {
  const {sel}=abInstElements();const d=dev;
  if(!sel||!abInstIsGolana(d)){abInstStatus('Keine GOLANA-Verbindung.');return;}
  const text=sel.value, num=Number(text);
  if(text===''||!Number.isInteger(num)||num<0||num>31)return;
  // core.h G_ROUTE=14: 0 CH1-4; 1 SEL; 2 CH5-8; 3 CH9-12; 4 CH13-16.
  const route=Number(d.dump.g?.[14]??0), channel=route>=2&&route<=4?4*(route-1):0;
  try{
    d.output.send([0xC0|channel,num]);
    abInstSelected=text;
    abInstStatus(`P${String(num+1).padStart(2,'0')} gesendet (MIDI Program Change). FM-1 auf STOP prüfen; PLAY/REC sperrt Programmwechsel.`);
    // The Felucca DUMP cannot reveal the active Golana program number; it can show its parameters.
    await new Promise(resolve=>setTimeout(resolve,180));
    if(dev===d){try{await afterSoundChange('ready',d);}catch(e){abInstStatus('P'+String(num+1).padStart(2,'0')+' gesendet; Rücklesen fehlgeschlagen: '+e.message);}}
  }catch(e){abInstStatus('MIDI-Ausgabe fehlgeschlagen: '+e.message);}
}
function abInstStart(){
  const {sel,refresh}=abInstElements();
  if(!sel||!refresh)return;
  sel.addEventListener('change',abInstChoose);
  refresh.addEventListener('click',abInstReadNames);
  abInstRender();
}
'''
ANCHORS={
    'browser':'<div class="browser" id="browser">',
    'script':'$("connect").addEventListener("click", () => {',
    'buildUI':'function buildUI() {',
    'renderBank':'function renderBank() {',
    'afterSoundChange':'async function afterSoundChange(msg = "ready", d = dev) {',
}
class Refused(Exception):pass

def strip_v1(src):
    mark='/* analogBEAST PROGRAM P01-P32 FIX: firmware golana_ui.c */'
    if mark not in src:return src
    if src.count(mark)!=1 or src.count('<style id="ab-program-selector-style">')!=1:raise Refused('V1-Programmpatch unvollständig')
    src,n=re.subn(r'<style id="ab-program-selector-style">.*?</style>\s*','',src,count=1,flags=re.S)
    if n!=1:raise Refused('V1-CSS nicht eindeutig')
    src,n=re.subn(r'/\* analogBEAST PROGRAM P01-P32 FIX: firmware golana_ui\.c \*/.*?(?=function buildGroups\(\) \{)','',src,count=1,flags=re.S)
    if n!=1:raise Refused('V1-Skript nicht eindeutig')
    for key in ('function buildUI() {','function renderBank() {'):
        old=key+'\n  abProgramUi();'
        if src.count(old)!=1:raise Refused('V1-Rückbau ist nicht eindeutig: '+key)
        src=src.replace(old,key,1)
    return src

def change(src):
    if MARK in src:
        if src.count(MARK)==1 and src.count('id="ab-instrument-program"')==1 and 'function abInstStart()' in src and 'abInstStart();' in src:
            return src,'ALREADY'
        raise Refused('Unvollständiger V2-Patch; kein automatisches Überschreiben')
    src=strip_v1(src)
    for label,value in ANCHORS.items():
        if src.count(value)!=1:raise Refused(f'Quellstand abweichend: {label} ({src.count(value)} statt 1)')
    if src.count('</head>')!=1: raise Refused('HTML Head nicht eindeutig')
    if src.count('id="ab-instruments"') or src.count('id="ab-instrument-program"'):
        raise Refused('Programmwähler bereits vorhanden, aber nicht als V2 registriert')
    if '/* analogBEAST Studio v1.0:' not in src:raise Refused('analogBEAST Studio nicht installiert')
    if '<!-- analogBEAST v1.0 Studio ARP cleanup -->' not in src:raise Refused('ARP Cleanup nicht installiert')
    # Static markup: the P01-P32 selector remains visible, even when the script is broken.
    src=src.replace(ANCHORS['browser'],ANCHORS['browser']+'\n'+MARK+'\n'+PANEL,1)
    src=src.replace('</head>',STYLE+'\n</head>',1)
    src=src.replace(ANCHORS['script'],JS+'\nabInstStart();\n'+ANCHORS['script'],1)
    src=src.replace(ANCHORS['buildUI'],ANCHORS['buildUI']+'\n  abInstRender();',1)
    src=src.replace(ANCHORS['renderBank'],ANCHORS['renderBank']+'\n  abInstRender();',1)
    return src,'PATCHED'

def main():
    ap=argparse.ArgumentParser();g=ap.add_mutually_exclusive_group()
    g.add_argument('--check-only',action='store_true');g.add_argument('--restore',action='store_true')
    a=ap.parse_args()
    try:
        if a.restore:
            if not TARGET.exists() or not BACKUP.exists():raise Refused('Datei oder Backup fehlt')
            if MARK not in TARGET.read_text(encoding='utf-8'):raise Refused('V2 nicht aktiv; Rückbau verweigert')
            shutil.copy2(TARGET,TARGET.with_name('editor.html.ab-programs-v2-undo-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')))
            shutil.copy2(BACKUP,TARGET)
            print('RESTORED: Ursprungseditor aus Backup');return 0
        if not TARGET.exists():raise Refused('web/editor.html fehlt; ZIP in C:\\FM1\\analogBEAST entpacken')
        s=TARGET.read_text(encoding='utf-8');out,status=change(s)
        if a.check_only:
            print('PASS: '+status+'; echter GOLANA-P01–P32-Wähler; kein Schreibvorgang');return 0
        if status=='ALREADY':print('ALREADY: V2 installiert');return 0
        if BACKUP.exists():raise Refused('Sicherung existiert bereits; Überschreiben verweigert')
        shutil.copy2(TARGET,BACKUP)
        temp=TARGET.with_suffix('.tmp')
        try:
            temp.write_text(out,encoding='utf-8');temp.replace(TARGET)
        except Exception:
            temp.unlink(missing_ok=True);shutil.copy2(BACKUP,TARGET);raise
        print('PATCHED: web/editor.html')
        print('BACKUP: web/editor.html.before-ab-instrument-v2')
        print('PASS: sichtbar P01–P32; 16 Werksnamen + 16 INIT/Benutzernamen; Engine-Felucca-Auswahl verborgen')
        print('NO FIRMWARE / FLASH / NOR / SONG / MIDI WRITE FROM INSTALLER')
        return 0
    except (Refused,OSError,UnicodeError) as e:
        print('REFUSED:',e,file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
