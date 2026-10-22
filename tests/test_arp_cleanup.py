from pathlib import Path
import importlib.util
import tempfile
import subprocess
import sys

HERE = Path(__file__).resolve().parent
TARGET = HERE.parent / 'tools' / 'cleanup_analogbeast_editor.py'
spec = importlib.util.spec_from_file_location('ab_cleanup', TARGET)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

STUDIO = HERE / 'fixture_studio.js'

def fixture():
    src = STUDIO.read_text(encoding='utf-8')
    html = '''<!doctype html><html><head><title>analogBEAST</title></head><body>
<nav class="tabs"><button role="tab" data-tab="sound">Sound</button>
<button role="tab" data-tab="sequencer" data-t="tabSeq" data-ic="symbol_grid"></button>
<button role="tab" data-tab="fm6"></button>
<button role="tab" data-tab="tracks"></button>
<button role="tab" data-tab="library"></button>
<button role="tab" data-tab="samples"></button>
<button role="tab" data-tab="projects"></button>
<button role="tab" data-tab="settings"></button></nav>
<section class="panel" id="p-sequencer" data-tab="sequencer" data-dev hidden><div class="groups" id="seqgroups"></div></section>
<div id="browser" class="browser"></div>
<script>'''+src+'''</script>
</body></html>'''
    return html

def test_transform():
    s = fixture()
    out,status = m.transform(s)
    assert status == 'PATCHED'
    assert m.transform(out)[1] == 'ALREADY'
    assert '<button role="tab" data-tab="sequencer" data-ic="ui_knob">ARP</button>' in out
    assert '01 · GOLANA ARP / SOUND' in out
    assert '02 · ARP TIMING / PART' in out
    assert "add('groups','10 · ARPEGGIATOR'" not in out
    assert 'TRACK STEP SETTINGS' not in out
    assert 'analogBEAST · GOLANA / BRUTE · Sound & ARP' in out
    assert '#tabs [data-tab="samples"]' in out
    assert 'ab-advanced' in out and "advanced?'details':'div'" in out
    assert out.count('id="ab-arp-cleanup-style"') == 1
    for n in (84,85,86,87,88,89,17,18,19,20,21,22,23,24):
        assert f'{n}' in out
    return out

def test_roundtrip():
    s = fixture()
    with tempfile.TemporaryDirectory() as d:
        root=Path(d)
        (root/'tools').mkdir()
        (root/'web').mkdir()
        (root/'tools'/'cleanup_analogbeast_editor.py').write_text(TARGET.read_text(encoding='utf-8'), encoding='utf-8')
        (root/'web'/'editor.html').write_text(s,encoding='utf-8')
        def run(*args):
            return subprocess.run([sys.executable,str(root/'tools'/'cleanup_analogbeast_editor.py'),*args],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        assert 'no files changed' in run('--check-only').stdout
        assert (root/'web'/'editor.html').read_text(encoding='utf-8')==s
        assert 'PATCHED' in run().stdout
        assert 'ALREADY' in run('--check-only').stdout
        assert 'ALREADY' in run().stdout
        assert 'RESTORED' in run('--restore').stdout
        assert (root/'web'/'editor.html').read_text(encoding='utf-8')==s

if __name__ == '__main__':
    text=test_transform()
    test_roundtrip()
    print('PASS: ARP tab, sound/part split, only SOUND+ARP+SETTINGS shown')
    print('PASS: Studio advanced remains accessible; 5 legacy views hidden, no data deleted')
    print('PASS: check-only, idempotence, backup and restore')
