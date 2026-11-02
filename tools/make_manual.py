#!/usr/bin/env python3
"""Builds the PDF manual: docs/manual.txt + the interface tour (docs/tour, made from the real compiled editor by tools/make_tour.py)
+ the factory presets read from src/editor/Presets.cpp.   Usage: python3 tools/make_manual.py docs/manual.txt LapCat2A-manual.pdf"""
import json, os, re, sys
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.lib.utils import ImageReader
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image, PageBreak, KeepTogether
txt, out = sys.argv[1:3]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tdir = os.path.join(root, 'docs', 'tour')
PINK, TEAL, INK, DIM, AMBER, PANEL = (colors.HexColor(c) for c in ('#d6126f', '#0a8f78', '#1c1d22', '#555a66', '#b8780a', '#eef0f4'))
st = lambda n, **k: ParagraphStyle(n, fontName=k.pop('font', 'Helvetica'), **k)
H1 = st('h1', fontSize=26, leading=30, textColor=PINK, font='Helvetica-Bold')
H2 = st('h2', fontSize=14, leading=18, textColor=AMBER, font='Helvetica-Bold', spaceBefore=10, spaceAfter=4)
H3 = st('h3', fontSize=11.5, leading=15, textColor=TEAL, font='Helvetica-Bold', spaceBefore=6, spaceAfter=3)
B = st('b', fontSize=10, leading=14, textColor=INK)
SM = st('s', fontSize=8.5, leading=11.5, textColor=DIM)
CELL = st('c', fontSize=9, leading=12, textColor=INK)
CELLB = st('cb', fontSize=9, leading=12, textColor=PINK, font='Helvetica-Bold')
esc = lambda t: t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
sec, cur = {}, None
for line in open(txt, encoding='utf-8').read().splitlines():
    if line.startswith('# '): cur = line[2:].strip(); sec[cur] = []
    elif cur is not None: sec[cur].append(line)
para = lambda k: [p.strip() for p in '\n'.join(sec.get(k, [])).split('\n\n') if p.strip()]
bullets = lambda k: [l[2:].strip() for l in sec.get(k, []) if l.startswith('- ')]
def img(name, width, maxh=None):
    p = os.path.join(tdir, name); w, h = ImageReader(p).getSize()
    if maxh and width * h / w > maxh: width = maxh * w / h
    return Image(p, width=width, height=width * h / w)
def tbl(rows):
    t = Table(rows, colWidths=[10 * mm, 38 * mm, 122 * mm])
    t.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('BACKGROUND', (0, 0), (-1, -1), PANEL), ('LINEBELOW', (0, 0), (-1, -1), 0.5, colors.white), ('TOPPADDING', (0, 0), (-1, -1), 4), ('BOTTOMPADDING', (0, 0), (-1, -1), 4)]))
    return t
tour = json.load(open(os.path.join(tdir, 'tour.json')))
rows = lambda keys: [[Paragraph(t['n'], CELLB), Paragraph(esc(t['name']), CELLB), Paragraph(esc(t['tip']), CELL)] for t in tour if keys(t['n'])]
story = [Paragraph('LapCat2A', H1), Paragraph('Manual . free and open source (AGPL-3.0-or-later) . github.com/SpeakingColors/lapcat2a . by Studio 87', SM), Spacer(1, 6)]
for p in para('Intro'): story += [Paragraph(esc(p), B), Spacer(1, 4)]
story.append(Paragraph('Quick start', H2))
for t in bullets('Quick start'): story.append(Paragraph('&bull; ' + esc(t), B))
story.append(Paragraph('Recommendations', H2))
for t in bullets('Recommendations'): story.append(Paragraph('&bull; ' + esc(t), B))
story += [PageBreak(), Paragraph('Interface tour', H2),
  Paragraph(esc('Pink numbers mark controls and teal letters mark displays. The picture is the real compiled plugin window, drawn offscreen by the build tools. Every number is explained in the tables below.'), B), Spacer(1, 4),
  img('tour-full.png', 175 * mm), Spacer(1, 4),
  Paragraph('Signal flow', H3), Paragraph('Input, sidechain filter (HF Emphasis), T4 cell, Gain, tube stage, output transformer, Mix, output.', B),
  Paragraph('Header bar', H3), tbl(rows(lambda n: n.isdigit() and int(n) <= 8)),
  PageBreak(), Paragraph('Hardware row: Gain, meter, Peak Reduction', H2), img('crop-hw.png', 175 * mm, 80 * mm), Spacer(1, 4),
  Paragraph(esc('The meter shows gain reduction in dB (0 at the right) with VU style needle ballistics, or the output level in VU when you pick +10 or +4. Gain never moves the gain reduction needle.'), B),
  tbl(rows(lambda n: n in ('9', '10', '13', '14', '15', 'A'))),
  Paragraph('Displays and the controls under them', H2), img('crop-disp.png', 175 * mm, 80 * mm), Spacer(1, 4),
  Paragraph(esc('Transfer curve: output level against input level in dB, before Gain, yellow dashed line at the threshold, magenta dot is the live signal; drag it sideways to move Peak Reduction. Gain reduction history: the last six seconds, 0 to -20 dB. Sidechain filter: what the detector listens to, 20 Hz to 20 kHz, 0 to -18 dB; drag up or down to move HF Emphasis.'), B),
  tbl(rows(lambda n: n in ('B', 'C', 'D', '11', '12', '16', '17'))),
  Paragraph('Level strips', H2), img('crop-meters.png', 35 * mm, 100 * mm),
  Paragraph(esc('Both strips run from -60 to +6 dB in neon blue, purple (above -12 dB) and pink (above 0 dB). The marker at the top latches when a peak reaches 0 dB; click the strip to clear it. The numbers under the strips are the highest recent peaks in dB.'), B),
  tbl(rows(lambda n: n in ('E', 'F', 'G', 'H'))),
  PageBreak(), Paragraph('Dialogs', H2)]
for f, cap in (('dlg-settings.png', 'Settings: info bubbles, power saving, noise floor, colour look (three looks), reset window.'), ('dlg-presets.png', 'Preset browser: pages of eight, search, type chips, load, copy and rename, delete, export, import.'), ('dlg-guide3.png', 'Guide, third tab: what is verified from published sources and what is our own tuning.')):
    story += [KeepTogether([Paragraph(esc(cap), H3), img(f, 150 * mm, 105 * mm), Spacer(1, 4)])]
src = open(os.path.join(root, 'src', 'editor', 'Presets.cpp'), encoding='utf-8').read()
prow = [[Paragraph('<b>Preset</b>', CELL), Paragraph('<b>Type</b>', CELL), Paragraph('<b>Settings</b>', CELL)]]
for line in src.splitlines():
    m = re.search(r'mk \("([^"]+)", "([^"]+)"', line)
    if not m: continue
    kv = re.findall(r'\{ "(\w+)", ([\d.]+) \}', line)
    prow.append([Paragraph(esc(m.group(1)), CELL), Paragraph(m.group(2), CELL), Paragraph(esc(', '.join(f'{k} {v}' for k, v in kv)), CELL)])
tb = Table(prow, colWidths=[50 * mm, 25 * mm, 95 * mm]); tb.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LINEBELOW', (0, 0), (-1, -1), 0.4, DIM)]))
story += [PageBreak(), Paragraph('Factory presets', H2), Paragraph(esc('Settings not listed use the defaults: Peak 35, Gain 20, Compress, HF Emphasis 0, Mix 100.'), SM), tb,
          PageBreak(), Paragraph('Under the hood', H2)]
for p in para('Under the hood'): story += [Paragraph(esc(p), B), Spacer(1, 5)]
story += [Paragraph('Unsigned installers', H2)] + [Paragraph(esc(p), B) for p in para('Unsigned installers')]
SimpleDocTemplate(out, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=16 * mm, title='LapCat2A manual', author='Studio 87').build(story)
print('wrote', out)
