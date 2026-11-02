#!/usr/bin/env python3
"""Draws the numbered tour from the REAL compiled editor render (docs/tour/window.png, 2x) and src/editor/Layout.h.
Writes docs/tour/tour-full.png, docs/tour/crop-*.png and docs/tour/tour.json (control list with the info-bubble text)."""
import json, re, sys
from PIL import Image, ImageDraw, ImageFont
root = sys.argv[1] if len(sys.argv) > 1 else '.'
src = open(f'{root}/src/editor/Layout.h').read()
rc = {m.group(1): tuple(float(v.replace('f','')) for v in m.group(2).split(',')) for m in re.finditer(r'constexpr Rc (\w+) = \{ ([^}]+) \};', src)}
tip = {m.group(1): m.group(2) for m in re.finditer(r'constexpr const char\* (tip_\w+) = "([^"]*)";', src)}
segs = re.findall(r'\{ \{ ([\d.f, ]+) \}, "([^"]+)", "([^"]*)" \}', src)
im = Image.open(f'{root}/docs/tour/window.png').convert('RGB')
S = im.width / 1100.0
try:
    F = ImageFont.truetype(f'{root}/assets/fonts/BarlowCondensed-Bold.ttf', int(15 * S))
except Exception:
    F = ImageFont.load_default()
items = []  # (key, label, rect, text)
def add(key, label, r, text): items.append((key, label, r, text))
for k, l, n in [('pPrev', 'Previous preset', 'tip_pPrev'), ('pName', 'Preset name', 'tip_pName'), ('pNext', 'Next preset', 'tip_pNext'), ('pSave', 'Save', 'tip_pSave'),
                ('bBypass', 'Bypass', 'tip_bBypass'), ('bGuide', 'Guide', 'tip_bGuide'), ('bSettings', 'Settings', 'tip_bSettings'), ('bAbout', 'About & Support', 'tip_bAbout')]:
    add(k, l, rc[k], tip[n])
kn = {'Gain': 'Gain', 'Peak Reduction': 'Peak Reduction', 'Mix': 'Mix', 'HF Emphasis': 'HF Emphasis'}
kr = re.findall(r'\{ "(\w[\w ]*)", (true|false), \{ [^}]*\}, \{ [^}]*\}, \{ ([^}]*)\}, \{ [^}]*\}, "[^"]*", "([^"]*)" \}', src)
for name, big, knob, t in kr:
    x, y, w, h = [float(v.replace('f', '')) for v in knob.split(',')]
    add('k' + name, name, (x, y, w, h), t)
for (r, txt, t) in segs:
    x, y, w, h = [float(v.replace('f', '')) for v in r.split(',')]
    add('s' + txt, ('Meter ' if txt in ('GR', '+10', '+4') else 'Ratio ') + txt, (x, y, w, h), t)
disp = [('cvVU', 'Meter face (VU)', 'tip_cvVU'), ('cvCurve', 'Transfer curve', 'tip_cvCurve'), ('cvHist', 'Gain reduction history', 'tip_cvHist'), ('cvFilt', 'Sidechain filter', 'tip_cvFilt'),
        ('cvIn', 'Input level strip', 'tip_cvIn'), ('cvOut', 'Output level strip', 'tip_cvOut'), ('pkIn', 'Input peak readout', 'tip_pkIn'), ('pkOut', 'Output peak readout', 'tip_pkOut')]
out = []
d = ImageDraw.Draw(im)
def badge(cx, cy, s, col):
    r = 11 * S * 0.9
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col, outline=(0, 0, 0), width=max(1, int(S)))
    tw = d.textlength(s, font=F)
    d.text((cx - tw / 2, cy - 9 * S * 0.9), s, font=F, fill=(20, 10, 20))
n = 0
for key, label, r, text in items:
    n += 1
    x, y, w, h = r
    cx, cy = (x - 2) * S, (y - 2) * S
    if key in ('kGain', 'kPeak Reduction'): cx, cy = (x + 4) * S, (y + 4) * S
    if key.startswith('s'): cx, cy = (x + w / 2) * S, (y + h + 9) * S
    if key in ('kMix', 'kHF Emphasis'): cx, cy = (x - 6) * S, (y + 4) * S
    badge(cx, cy, str(n), (255, 123, 177))
    out.append({'n': str(n), 'name': label, 'tip': text})
for i, (key, label, n_) in enumerate(disp):
    letter = 'ABCDEFGH'[i]
    x, y, w, h = rc[key]
    px = (x - 12) * S if key.startswith('pk') else (x + 10) * S
    py = (y + 7) * S if key.startswith('pk') else (y + 10) * S
    badge(px, py, letter, (2, 235, 186))
    out.append({'n': letter, 'name': label, 'tip': tip[n_]})
im.save(f'{root}/docs/tour/tour-full.png')
base = Image.open(f'{root}/docs/tour/window.png').convert('RGB')
def crop(name, rect):
    x0, y0, x1, y1 = [int(v * S) for v in rect]
    base.crop((x0, y0, x1, y1)).save(f'{root}/docs/tour/crop-{name}.png')
crop('header', (52, 0, 1048, 46)); crop('hw', (52, 46, 956, 338)); crop('disp', (52, 338, 956, 570)); crop('meters', (956, 46, 1048, 570))
json.dump(out, open(f'{root}/docs/tour/tour.json', 'w'), indent=1)
print(len(out), 'entries')
