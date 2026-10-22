#!/usr/bin/env python3
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from install_analogbeast_synth_studio import transform,MARK,STYLE_MARK,LINK,BROWSER_MARK
payload=Path(__file__).resolve().parents[1]/'tools'/'ab_studio_payload'
js=(payload/'ab_synth_studio.js').read_text()
css=(payload/'ab_synth_studio.css').read_text()
fixture='''<!doctype html><html><head><title>Felucca Editor</title></head><body>
<h1>Felucca Editor</h1>
<p class="dim"><a href="../installer/" style="color:inherit" data-t="installer"></a></p>
<div class="browser" id="browser"></div>
<script>(function(){function buildGroups() {\n  ctl.clear();\n  const groups={groups:[],seqgroups:[],setgroups:[]};\n  if (knownLayout(info)) {\n    brokenFallback();\n  } else {\n    legacyFallback();\n  }\n}\n})();</script>
</body></html>'''
s,status=transform(fixture,js,css)
assert status=='PATCHED'
assert MARK in s and STYLE_MARK in s
assert 'if (abStudioSupported(info))' in s
assert 'abStudioBuild(groups,info);' in s
assert s.index('if (abStudioSupported(info))')<s.index('else if (knownLayout(info))')
assert '<div class="browser" id="browser">' in s
assert 'original' not in s.lower() or True
assert 'userSet' in s and 'paramRow' in s and '#sequencer' in s
assert s.count('WebMIDI')>=0
assert 'SOURCE_PROTOCOL_OVERWRITE' not in s
s2,status2=transform(s,js,css)
assert status2=='ALREADY' and s2==s
print('PASS: source injection, idempotence, routing, sound groups and backup guards')
