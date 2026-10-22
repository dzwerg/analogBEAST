import importlib.util, pathlib, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('f',root/'tools'/'fix_analogbeast_instrument_ui.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
fixture='''<html><head></head><body>
<div class="browser" id="browser"><label><select id="engine"></select></label><label><select id="preset"></select></label></div>
<!-- analogBEAST v1.0 Studio ARP cleanup -->
<script>/* analogBEAST Studio v1.0: */
function buildUI() {return 1;}
function renderBank() {return 1;}
async function afterSoundChange(msg = "ready", d = dev) {return 1;}
$("connect").addEventListener("click", () => {});
</script></body></html>'''
result,status=m.change(fixture)
assert status=='PATCHED'
assert 'P01 · FAT BASS' in result and 'P32 · INIT' in result
assert result.count('id="ab-instrument-program"')==1
assert 'abInstStart();' in result
assert 'function buildUI() {\n  abInstRender();' in result
assert 'function renderBank() {\n  abInstRender();' in result
assert 'abInstChoose' in result and '0xC0|channel,num' in result
assert m.change(result)[1]=='ALREADY'
legacy_css='<style id="ab-program-selector-style">abc</style>'
legacy_js='/* analogBEAST PROGRAM P01-P32 FIX: firmware golana_ui.c */\nconst AB_PROGRAM_FACTORY=[];\n'
v1=fixture.replace('</head>',legacy_css+'</head>')
v1=v1.replace('function buildUI() {','function buildUI() {\n  abProgramUi();')
v1=v1.replace('function renderBank() {','function renderBank() {\n  abProgramUi();')
v1=v1.replace('$("connect").addEventListener',legacy_js+'$("connect").addEventListener')
# Real V1 inserts its JS before function buildGroups, so emulate that anchor.
v1=v1.replace('function buildUI()', 'function buildGroups() {}\nfunction buildUI()')
v1=v1.replace(legacy_js+'$("connect")', '$("connect")')
v1=v1.replace('function buildGroups() {}',''+legacy_js+'function buildGroups() {}')
up,status=m.change(v1)
assert status=='PATCHED' and 'abProgramUi' not in up and 'ab-program-selector-style' not in up
assert len(m.FACTORY)==16
print('PASS: P01–P32 source names, static visible selector, MIDI PC, idempotency, legacy-V1 cleanup')
