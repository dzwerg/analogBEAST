#!/usr/bin/env python3
"""Install analogBEAST Studio in existing web/editor.html (no device access).

Run from C:\\FM1\\analogBEAST:
  py tools\\install_analogbeast_synth_studio.py --check-only
  py tools\\install_analogbeast_synth_studio.py
  py tools\\install_analogbeast_synth_studio.py --restore

This installer intentionally changes only web/editor.html and web/installer/index.html.
"""
import argparse
from datetime import datetime
from pathlib import Path
import re
import shutil
import sys

ROOT=Path(__file__).resolve().parent.parent
WEB=ROOT/'web'
EDITOR=WEB/'editor.html'
BRIDGE=WEB/'installer'/'index.html'
PAYLOAD=Path(__file__).resolve().parent/'ab_studio_payload'
MARK='/* analogBEAST Studio v1.0:'
STYLE_MARK='/* analogBEAST Studio. Scoped'
ANCHOR='function buildGroups() {'
LEGACY_ANCHOR='  if (knownLayout(info)) {'
LINK='<p class="dim"><a href="../installer/" style="color:inherit" data-t="installer"></a></p>'
REPLACEMENT_LINK='<p class="dim ab-installer-link"><a href="./installer/" id="abstudio-installer">Installer / Release</a> · <span>analogBEAST basiert auf Felucca (GPL-3.0)</span></p>'
BROWSER_MARK='<div class="browser" id="browser">'
HERO='''<div id="abstudio-hero">
      <div><h2>analogBEAST · SYNTH STUDIO</h2>
        <p id="abstudio-mode">BRUTE / MIKROKORG · Hardware-Synthese</p>
        <span id="abstudio-device">Warte auf Verbindung</span></div>
      <div class="ab-actions"><a href="#sequencer" title="Patterns, SONG und ALBUM öffnen">SEQ · SONG · ALBUM →</a></div>
    </div>'''
FOOT_SCRIPT='''<script id="abstudio-release-nav">
(function(){const link=document.getElementById('abstudio-installer');
 if(link&&/\\/webapp\\/editor\\//i.test(location.pathname)) link.href='../installer/';
})();
</script>
'''
BRIDGE_HTML='''<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>analogBEAST Installer · Lokal</title>
<style>body{background:#0d1016;color:#e9eef5;font:16px/1.6 system-ui;margin:5vw auto;max-width:760px;padding:0 16px}
h1{color:#f5a14c}main{background:#1a202a;padding:26px;border:1px solid #39414d;border-radius:12px}
code,pre{background:#10151d;padding:5px;border-radius:5px;color:#ffcc91;overflow:auto}a{color:#ffc18a}</style></head>
<body><main><h1>analogBEAST v1.0 · Installer</h1>
<p>Du befindest dich im <strong>lokalen Quell-Webserver</strong>. Der vollständige OTA-Installer wird vom offiziellen Projekt-Buildskript als Release-Site generiert. Diese Seite verhindert einen irreführenden 404-Fehler; sie führt <strong>keinen</strong> Flash durch.</p>
<p>Am PC kannst du das geprüfte Firmwarepaket weiterhin über PowerShell installieren:</p>
<pre>cd C:\\FM1\\analogBEAST
py tools\\fm1_install.py ".\\build\\analogBEAST-v1.0.fwsc" --port "analogBEAST 3"</pre>
<p>Für den GitHub-Pages-Release-Installer die Site mit <code>web/make_site.py</code> erzeugen und aus dem generierten Verzeichnis hosten. Im veröffentlichten Paket führt der Installer-Link direkt zur vollständigen Web-OTA-Seite.</p>
<p><a href="../editor.html#sound">← Zurück zum Synth-Editor</a></p></main></body></html>
'''

class InstallError(RuntimeError):pass

def transform(src,js,css):
    if MARK in src or STYLE_MARK in src:
        if all(x in src for x in (MARK,STYLE_MARK, 'abstudio-hero', 'abstudio-installer')):
            return src, 'ALREADY'
        raise InstallError('Unvollständiger Studio-Patch – manuelle Prüfung nötig')
    required={
        'Editor title':'<title>Felucca Editor</title>',
        'Editor heading':'<h1>Felucca Editor</h1>',
        'Felucca installer link':LINK,
        'Sound browser':BROWSER_MARK,
        'Dynamic buildGroups':ANCHOR,
        'Layout fallback':LEGACY_ANCHOR,
        'End of <head>':'</head>',
        'End of <body>':'</body>',
    }
    for label,needle in required.items():
        count=src.count(needle)
        if count!=1:raise InstallError(f'{label}: erwarteter Anker fehlt/mehrfach ({count})')
    # Ensure legacy branch we modify is within the buildGroups() function,
    # not a similarly named clause elsewhere.
    loc=src.index(ANCHOR); nextloc=src.find(LEGACY_ANCHOR,loc)
    if nextloc<loc or nextloc>loc+600:raise InstallError('Layout fallback nicht direkt in buildGroups')
    source=src.replace('<title>Felucca Editor</title>','<title>analogBEAST v1.0 | Synth Studio</title>')
    source=source.replace('<h1>Felucca Editor</h1>','<h1>analogBEAST <span style="font-size:.42em;color:#cad1db;font-weight:450">v1.0 · STUDIO</span></h1>')
    source=source.replace(LINK,REPLACEMENT_LINK)
    source=source.replace(BROWSER_MARK,HERO+'\n    '+BROWSER_MARK,1)
    source=source.replace('</head>','<style id="abstudio-theme">\n'+css+'\n</style>\n</head>',1)
    source=source.replace(ANCHOR,js+'\n'+ANCHOR,1)
    loc=source.index(ANCHOR);nextloc=source.find(LEGACY_ANCHOR,loc)
    source=source[:nextloc]+source[nextloc:].replace(LEGACY_ANCHOR,
        '  if (abStudioSupported(info)) {\n    abStudioBuild(groups,info);\n  } else if (knownLayout(info)) {',1)
    source=source.replace('</body>',FOOT_SCRIPT+'</body>',1)
    return source,'PATCHED'

def main():
    ap=argparse.ArgumentParser()
    mx=ap.add_mutually_exclusive_group()
    mx.add_argument('--check-only',action='store_true')
    mx.add_argument('--restore',action='store_true')
    a=ap.parse_args()
    orig=EDITOR.with_suffix('.html.before-abstudio-v1')
    try:
        if a.restore:
            if not orig.is_file():raise InstallError(f'Keine Sicherung: {orig}')
            if not EDITOR.is_file() or MARK not in EDITOR.read_text(encoding='utf-8'):
                raise InstallError('Editor wurde verändert; Restore verweigert, bitte selbst diffen')
            shutil.copy2(orig,EDITOR)
            print('RESTORED: editor.html from .before-abstudio-v1')
            print('INFO: lokale Installer-Infoseite bleibt erhalten')
            return
        if not EDITOR.is_file():raise InstallError('Fehlt: web/editor.html (Projektordner prüfen)')
        src=EDITOR.read_text(encoding='utf-8')
        js=(PAYLOAD/'ab_synth_studio.js').read_text(encoding='utf-8')
        css=(PAYLOAD/'ab_synth_studio.css').read_text(encoding='utf-8')
        out,status=transform(src,js,css)
        if a.check_only:
            print(f'PASS: {status}; Felucca-WebMIDI-Protocol bleibt erhalten; kein Schreibvorgang')
            return
        if status=='ALREADY':
            print('ALREADY: synth studio already installed')
            return
        if orig.exists():raise InstallError('Sicherungsdatei existiert schon – kein Überschreiben')
        if BRIDGE.exists():raise InstallError('web/installer/index.html existiert bereits – nicht überschreiben')
        shutil.copy2(EDITOR,orig)
        # Use a temp file + replacement so failures don't leave partial HTML.
        tmp=EDITOR.with_suffix('.html.abstudio-tmp')
        try:
            tmp.write_text(out,encoding='utf-8')
            tmp.replace(EDITOR)
            BRIDGE.parent.mkdir(parents=True,exist_ok=True)
            BRIDGE.write_text(BRIDGE_HTML,encoding='utf-8')
        except Exception:
            if tmp.exists():tmp.unlink()
            shutil.copy2(orig,EDITOR)
            raise
        print('PATCHED: web/editor.html (new analogBEAST Sound Studio cards)')
        print('CREATED: web/installer/index.html (local informative page, no flashing)')
        print('BACKUP: web/editor.html.before-abstudio-v1')
        print('PASS: firmware descriptor groups + original WebMIDI/SEQ unchanged')
        print('NO MIDI, NOR, FLASH or FIRMWARE operations')
    except InstallError as e:
        print('REFUSED:',e,file=sys.stderr);sys.exit(2)

if __name__=='__main__':main()
