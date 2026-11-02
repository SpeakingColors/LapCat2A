#!/usr/bin/env python3
"""Reads every position, size, label, tooltip and text style of the approved mockup from its live DOM
(Playwright + Chromium) and writes docs/mockup/layout.json. tools/gen_layout.py turns that into
src/editor/Layout.h, so the native window cannot drift from the mockup. Run from the repo root.
The Google font link is blocked and the bundled fonts in assets/fonts are used instead (same files as the plugin)."""
import asyncio, json, os
from playwright.async_api import async_playwright

SRC = 'docs/mockup/LapCat2A.html'
fonts = '''
@font-face{font-family:'Barlow Condensed';font-weight:500;font-style:normal;src:url(file://%(r)s/assets/fonts/BarlowCondensed-Medium.ttf)}
@font-face{font-family:'Barlow Condensed';font-weight:600;font-style:normal;src:url(file://%(r)s/assets/fonts/BarlowCondensed-SemiBold.ttf)}
@font-face{font-family:'Barlow Condensed';font-weight:700;font-style:normal;src:url(file://%(r)s/assets/fonts/BarlowCondensed-Bold.ttf)}
@font-face{font-family:'Barlow Condensed';font-weight:800;font-style:italic;src:url(file://%(r)s/assets/fonts/BarlowCondensed-ExtraBoldItalic.ttf)}
@font-face{font-family:'Barlow Condensed';font-weight:700;font-style:italic;src:url(file://%(r)s/assets/fonts/BarlowCondensed-ExtraBoldItalic.ttf)}
@font-face{font-family:'JetBrains Mono';font-weight:400;src:url(file://%(r)s/assets/fonts/JetBrainsMono-Regular.ttf)}
@font-face{font-family:'JetBrains Mono';font-weight:600;src:url(file://%(r)s/assets/fonts/JetBrainsMono-Regular.ttf)}
'''
JS = r"""
() => {
  const st=document.querySelector('#stage').getBoundingClientRect();
  const sc=st.width/1100;
  const R=e=>{const r=e.getBoundingClientRect();return [Math.round((r.left-st.left)/sc*100)/100,Math.round((r.top-st.top)/sc*100)/100,Math.round(r.width/sc*100)/100,Math.round(r.height/sc*100)/100]};
  const q=s=>document.querySelector(s);
  const out={rects:{},tips:{},texts:[]};
  const sel={cheekL:'#cheekL',cheekR:'#cheekR',main:'.main',hdr:'.hdr',body:'.body',left:'.left',hw:'#hw',disp:'.disp',rig:'.rig',meters:'.meters',foot:'.foot',
    logo:'.brand svg',name:'.brand .name',badge:'.brand .name i',s87:'.brand .s87',
    pPrev:'#pPrev',pName:'#pName',pNext:'#pNext',pSave:'#pSave',bBypass:'#bBypass',bGuide:'#bGuide',bSettings:'#bSettings',bAbout:'#bAbout',
    cGain:'#cGain',cPeak:'#cPeak',vuwrap:'.vuwrap',cvVU:'#cvVU',ears:'.ears',segMeter:'#segMeter',
    cvCurve:'#cvCurve',cvHist:'#cvHist',cvFilt:'#cvFilt',segMode:'#segMode',cMix:'#cMix',cEmph:'#cEmph',
    cvIn:'#cvIn',cvOut:'#cvOut',pkIn:'#pkIn',pkOut:'#pkOut',winsz:'#winsz'};
  for(const [k,s] of Object.entries(sel)){const e=q(s); if(!e){out.rects[k]=null;continue} out.rects[k]=R(e); if(e.dataset&&e.dataset.tip) out.tips[k]=e.dataset.tip}
  out.cheekTip=q('#cheekL').dataset.tip;
  // knobs
  out.knobs=[...document.querySelectorAll('.kcell')].map(c=>{const kn=c.querySelector('.knob');return{id:kn.getAttribute('aria-label'),big:!c.classList.contains('sm'),cell:R(c),lab:R(c.querySelector('.lab')),labText:c.querySelector('.lab').textContent,knob:R(kn),val:R(c.querySelector('.kval')),tip:kn.dataset.tip}});
  // segmented buttons
  const segs=(id)=>[...document.querySelectorAll(id+' button')].map(b=>({t:b.textContent,r:R(b),tip:b.dataset.tip}));
  out.segMeterBtns=segs('#segMeter'); out.segModeBtns=segs('#segMode');
  // captions
  const text=(e,kind)=>{const cs=getComputedStyle(e);return{kind,t:e.textContent,r:R(e),fs:parseFloat(cs.fontSize),mono:cs.fontFamily.indexOf('Barlow')<0,fw:parseInt(cs.fontWeight),ls:cs.letterSpacing==='normal'?0:parseFloat(cs.letterSpacing),tt:cs.textTransform,italic:cs.fontStyle==='italic',align:cs.textAlign,color:cs.color}};
  out.dcaps=[...document.querySelectorAll('.dcell .cap')].filter(e=>e.parentElement.classList.contains('dcell')).map(e=>text(e,'dcap'));
  out.swcaps=[...document.querySelectorAll('.swg .cap')].map(e=>text(e,'swcap'));
  out.mcaps=[...document.querySelectorAll('.meters .cap span')].map(e=>text(e,'mcap'));
  out.footTexts=[...document.querySelectorAll('.foot span')].map(e=>text(e,'foot'));
  out.nameText=text(q('.brand .name'),'name'); out.s87Text=text(q('.brand .s87'),'s87'); out.badgeText=text(q('.brand .name i'),'badge');
  out.btnText=text(q('#pPrev'),'btn'); out.pnameText=text(q('#pName'),'pname');
  out.segText=text(q('#segMode button'),'seg'); out.segTextMeter=text(q('#segMeter button'),'segm');
  out.kvalText=text(q('#cGain .kval'),'kval'); out.kvalSm=text(q('#cMix .kval'),'kvalsm');
  out.labBig=text(q('#cGain .lab'),'labbig'); out.labSm=text(q('#cMix .lab'),'labsm');
  out.pk=text(q('#pkIn'),'pk');
  out.tokens=getComputedStyle(document.querySelector('#app')).getPropertyValue('--face');
  return out;
}
"""

async def main():
    root = os.getcwd()
    src = open(SRC).read()
    src = '\n'.join(l for l in src.split('\n') if 'fonts.googleapis' not in l and 'fonts.gstatic' not in l)
    html = '<!doctype html><html><head><meta charset="utf-8"><style>' + fonts % {'r': root} + '</style></head><body>' + src + '</body></html>'
    open('/tmp/lap_wrapped.html', 'w').write(html)
    async with async_playwright() as p:
        b = await p.chromium.launch(args=['--allow-file-access-from-files'])
        pg = await b.new_page(viewport={'width': 1300, 'height': 900})
        await pg.goto('file:///tmp/lap_wrapped.html'); await pg.wait_for_timeout(1500)
        d = await pg.evaluate(JS)
        json.dump(d, open('docs/mockup/layout.json', 'w'), indent=1)
        await pg.screenshot(path='/tmp/lap_mockup_ref.png', clip={'x': 0, 'y': 0, 'width': 1300, 'height': 720})
        el = await pg.query_selector('#stage')
        await el.screenshot(path='/tmp/lap_mockup_stage.png')
        print(d['rects']['main'], d['rects']['cheekL'], len(d['knobs']), d['rects']['rig'])
        await b.close()
asyncio.run(main())
