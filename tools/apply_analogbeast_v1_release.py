#!/usr/bin/env python3
"""Apply analogBEAST v1.0 branding onto the EXACT v5 FINAL source.
Safety: checks all source hashes before changing anything, backs up every source, no flash access.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil

EXPECTED = json.loads('{"firmware/src/felucca.c": {"sha_before": "a14f8456186e004ad1a6c1630a44fbcb57204e9c9c540f391991fcb719ca75a2", "sha_after": "8a188c198138c97a8280cb64249fac1caab099ea1810e03e52f61803741907af", "replacements": [["#define FELUCCA_VERSION \\"v1.0\\"   /*", "#define FELUCCA_VERSION \\"v1.0\\"   /*"], ["#if FELUCCA_OTA && !FELUCCA_FLASH", "#define AB_BRAND_NAME \\"analogBEAST\\"   /* user-facing app name; OTA ID remains FM-1_901 */\\n#if FELUCCA_OTA && !FELUCCA_FLASH"]]}, "firmware/src/usb.c": {"sha_before": "4fc17a5267b80dd1191b7d091cad95468a3358da28bb0663c07a1d6b73c6b284", "sha_after": "d36bde01c721d83e5eab8a7de999359d7bdc510dbdcfe73cc4ddd165a3671ad1", "replacements": [["#else\\nstatic const uint8_t STR2[] = {16, 3, \'F\', 0, \'e\', 0, \'l\', 0, \'u\', 0, \'c\', 0, \'c\', 0, \'a\', 0};\\n#endif", "#else\\n#if GOLANA\\n/* USB product string: analogBEAST (UTF-16LE); keep VID/PID and update-loader identity unchanged. */\\nstatic const uint8_t STR2[] = {24, 3, \'a\', 0, \'n\', 0, \'a\', 0, \'l\', 0, \'o\', 0, \'g\', 0, \'B\', 0, \'E\', 0,\\n                               \'A\', 0, \'S\', 0, \'T\', 0};\\n#else\\nstatic const uint8_t STR2[] = {16, 3, \'F\', 0, \'e\', 0, \'l\', 0, \'u\', 0, \'c\', 0, \'c\', 0, \'a\', 0};\\n#endif\\n#endif"]]}, "firmware/src/ui_draw.c": {"sha_before": "cccbd9f9f3c057884d82e73a17a97d0117174938e65cb32494b093b295133122", "sha_after": "7ce058913db36e6c7c8a0e284269f3c5e517a62071efce9b3bbc6572c66b61a3", "replacements": [["static const char *const SPLASH_LINES[] = {\\"Golana\\", FELUCCA_VERSION, \\"by Gadlele\\", \\"based on Felucca\\"};", "static const char *const SPLASH_LINES[] = {AB_BRAND_NAME, FELUCCA_VERSION, \\"by Gadlele\\", \\"based on Felucca\\"};"]]}, "firmware/src/ui_menu.c": {"sha_before": "b7ffd7c485e7e924a082f06754fa7300fd72508a95063911cb74a266599354ad", "sha_after": "e3cf7a1b9945d70734aacdbf0971175e105f2252b02c36d194f1ce0bb4f30b74", "replacements": [["GOLANA ? \\"GOLANA\\" : \\"FELUCCA\\"", "GOLANA ? AB_BRAND_NAME : \\"FELUCCA\\""]]}, "firmware/src/editor.c": {"sha_before": "c8efb79ecdf2527c6af755e4c052169ffb788522e48074789f0d3511398803ab", "sha_after": "6271114c7b83656adaeea622ea52bd991821e6743110baf1e6016db7a9a40e8c", "replacements": [["        ed_str(\\"FELUCCA \\" FELUCCA_VERSION, 24);", "#if GOLANA\\n        ed_str(AB_BRAND_NAME \\" \\" FELUCCA_VERSION, 24);\\n#else\\n        ed_str(\\"FELUCCA \\" FELUCCA_VERSION, 24);\\n#endif"]]}, "firmware/src/console.c": {"sha_before": "22164dd4dfdfe1760465111f6c5f7e43e7eeba17716688f24a0cacbd4bedf892", "sha_after": "543d5b1c41dc6f30d1d336f48bbf7f669230e1c83263f7def43a55d535f32850", "replacements": [["    con_puts(\\"felucca \\");", "#if GOLANA\\n    con_puts(AB_BRAND_NAME \\" \\");\\n#else\\n    con_puts(\\"felucca \\");\\n#endif"]]}, "tools/build.py": {"sha_before": "72e5e1f1948abc820c479157627599b773462b1692f7949850dc31db6128cc03", "sha_after": "7fd2402dac334347d59123ac569c6122db46d2fe4edd1be71b0d2836f180364a", "replacements": [["    (OUT / name).write_bytes(pkg)", "    (OUT / name).write_bytes(pkg)\\n    if not a.release:\\n        # Brand the normal firmware artifact WITHOUT --release (which changes FM-1_901 to FM-1_910).\\n        if PRODUCT != \\"FM-1_901\\":\\n            raise SystemExit(\\"analogBEAST: unexpected OTA identity; refusing branded artifact\\")\\n        branded = OUT / \\"analogBEAST-v1.0.fwsc\\"\\n        branded.write_bytes(pkg)\\n        print(f\\"branded  {branded}  {len(pkg)} B, identity {PRODUCT}\\")"]]}, "tools/fm1_install.py": {"sha_before": "d8a8374931f0506b5e9fde1ead154eae0341390578dc92a5a0a4f9c16cfa7d63", "sha_after": "8c0b5e37ade5cfa06150733a5833ed324a6df43b955d424a48d59444db951c71", "replacements": [["PORT_RE = re.compile(r\\"fm-1|felucca|ota|composite|sinco|usb-midi\\", re.I)", "PORT_RE = re.compile(r\\"fm-1|felucca|analogbeast|ota|composite|sinco|usb-midi\\", re.I)"]]}, "tools/analogbeast_v5_patterns.py": {"sha_before": "51db04b8d8843f51af16bfa606ff7bbf30bfde1597dfdc6976ccbf3070a95908", "sha_after": "7b4bbad4feee9ee61b1386061098a4623f0c9a012ee6302f4e1ac998f994418f", "replacements": [["ap.add_argument(\'--in-port\',default=\'Felucca 3\')", "ap.add_argument(\'--in-port\',default=\'analogBEAST 3\')"], ["ap.add_argument(\'--out-port\',default=\'Felucca 4\')", "ap.add_argument(\'--out-port\',default=\'analogBEAST 4\')"]]}}')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check-only', action='store_true')
    opt = ap.parse_args()
    root = Path(__file__).resolve().parents[1]
    pending=[]
    for rel, r in EXPECTED.items():
        p=root/rel
        if not p.is_file(): raise SystemExit(f'MISSING: {p}')
        raw=p.read_bytes()
        sha=hashlib.sha256(raw).hexdigest()
        if sha == r['sha_after']:
            print(f'ALREADY: {rel}')
            continue
        if sha != r['sha_before']:
            raise SystemExit(f'REFUSED: {rel} has unexpected changes (SHA256 {sha}). Do not force. Send updated source.')
        t=raw.decode('utf8')
        for a,b in r['replacements']:
            if t.count(a)!=1: raise SystemExit(f'REFUSED: patch anchor ambiguous/missing: {rel}: {a[:70]}')
            t=t.replace(a,b)
        out=t.encode('utf8')
        if hashlib.sha256(out).hexdigest()!=r['sha_after']:
            raise SystemExit('REFUSED: patch result checksum mismatch: '+rel)
        pending.append((p,out))
    if opt.check_only:
        print(f'PASS: {len(pending)} patches ready, no sources modified, FM-1_901 unchanged')
        return
    for p,out in pending:
        bak=p.with_name(p.name+'.before-ab-v1-identity')
        if bak.exists() and bak.read_bytes()!=p.read_bytes():
            raise SystemExit(f'REFUSED: backup already exists with different content: {bak}')
    for p,out in pending:
        bak=p.with_name(p.name+'.before-ab-v1-identity')
        if not bak.exists(): shutil.copy2(p,bak)
        tmp=p.with_name(p.name+'.ab-v1-tmp')
        tmp.write_bytes(out)
        tmp.replace(p)
        print('PATCHED:', p.relative_to(root))
    print('PASS: analogBEAST v1.0 branding installed. No flash operations.')

if __name__ == '__main__': main()
