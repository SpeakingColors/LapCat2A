// Runs the mockup's own JS model (docs/mockup/LapCat2A.html, class Eng) and the C++ core on the same input and compares.
// Oversampling is off in both (the plugin uses a latency exact 4x oversampler instead of the mockup's 2x one) and the DC block is off.
// usage: node tests/parity.js path/to/lap_parity
const fs = require('fs'), cp = require('child_process'), os = require('os'), path = require('path');
const exe = process.argv[2];
const html = fs.readFileSync(path.join(__dirname, '..', 'docs', 'mockup', 'LapCat2A.html'), 'utf8');
const a = html.indexOf('const EMPH_FC'), b = html.indexOf('/* ============ Preview sources');
if (a < 0 || b < 0) { console.error('could not find the model in the mockup'); process.exit(1); }
const code = 'const D2L=db=>Math.pow(10,db*0.05),L2D=x=>20*Math.log10(Math.max(x,1e-9));' + html.slice(a, b) + ';return Eng;';
const Eng = new Function(code)();
const N = 48000 * 6;
const L = new Float32Array(N), R = new Float32Array(N);
let s = 1234567;
const rnd = () => ((s = (Math.imul(s, 1664525) + 1013904223) >>> 0) / 4294967296) * 2 - 1;
for (let i = 0; i < N; i++) {
  const t = i / 48000, env = 0.15 + 0.7 * Math.pow(0.5 + 0.5 * Math.sin(2 * Math.PI * 0.7 * t), 2), burst = (Math.floor(t * 3) % 2) ? 1 : 0.2;
  const v = env * burst * (0.5 * Math.sin(2 * Math.PI * 220 * t) + 0.3 * Math.sin(2 * Math.PI * 1760 * t) + 0.1 * rnd());
  L[i] = v; R[i] = v * 0.9 + 0.05 * Math.sin(2 * Math.PI * 330 * t);
}
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'lap-'));
const inF = path.join(tmp, 'in.f32'), outF = path.join(tmp, 'out.f32');
const inter = new Float32Array(N * 2); for (let i = 0; i < N; i++) { inter[2 * i] = L[i]; inter[2 * i + 1] = R[i]; }
fs.writeFileSync(inF, Buffer.from(inter.buffer));
const sets = [[35, 20, 0, 0, 100], [50, 14, 0, 0, 100], [58, 22, 1, 0, 100], [52, 20, 1, 25, 100], [75, 46, 1, 70, 100], [0, 40, 0, 0, 100], [100, 100, 1, 100, 100]];
let bad = 0;
for (const [peak, gain, mode, emph, mix] of sets) {
  const e = new Eng(); e.prepare(48000); e.os = false; e.setP({ peak, gain, mode, emph, mix, bypass: false, noise: false });
  const l = Float32Array.from(L), r = Float32Array.from(R);
  for (let p = 0; p < N; p += 512) e.process(l.subarray(p, Math.min(N, p + 512)), r.subarray(p, Math.min(N, p + 512)), Math.min(512, N - p));
  cp.execFileSync(exe, [inF, outF, String(peak), String(gain), String(mode), String(emph), String(mix)]);
  const o = new Float32Array(fs.readFileSync(outF).buffer.slice(0));
  let maxd = 0, ref = 0;
  for (let i = 0; i < N; i++) { maxd = Math.max(maxd, Math.abs(o[2 * i] - l[i]), Math.abs(o[2 * i + 1] - r[i])); ref = Math.max(ref, Math.abs(l[i])); }
  const ok = maxd < 2e-6 * Math.max(1, ref);
  if (!ok) bad++;
  console.log((ok ? 'ok    ' : 'FAIL  ') + `JS model and C++ core agree for Peak ${peak} Gain ${gain} ${mode ? 'Limit' : 'Compress'} Emph ${emph}: largest difference ${maxd.toExponential(2)} (output peak ${ref.toFixed(3)})`);
}
fs.rmSync(tmp, { recursive: true, force: true });
if (bad) { console.log(bad + ' PARITY CHECKS FAILED'); process.exit(1); }
console.log('PARITY OK: the C++ core reproduces the mockup model');
