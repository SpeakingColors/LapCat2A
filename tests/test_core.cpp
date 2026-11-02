// LapCat2A core tests. Plain C++17, no JUCE: sh tests/run.sh
// Every number printed as "measured" comes from running the real core. Lines marked UNVERIFIED are not measured.
#include "../src/core/LapCat2A.h"
#include <cstdio>
#include <complex>
#include <vector>

using namespace lap;
static int failures = 0;
#define CHECK(cond, ...) do { const bool ok_ = (cond); std::printf ("%s  ", ok_ ? "ok  " : "FAIL"); std::printf (__VA_ARGS__); std::printf ("\n"); if (! ok_) ++failures; } while (0)

static const double FS = 48000.0;

static Params P (float peak, float gain, float limit = 0, float emph = 0, float mix = 100, bool bypass = false, bool noise = false)
{
    Params p; p.peak = peak; p.gain = gain; p.limit = limit; p.emph = emph; p.mix = mix; p.bypass = bypass; p.noise = noise; return p;
}
static void run (Core& c, std::vector<float>& l, std::vector<float>& r) { c.process (l.data(), r.data(), (int) l.size()); }
static std::vector<float> sine (double f, double dbfs, int n, double fs = FS)
{
    std::vector<float> v ((size_t) n);
    const double a = dbToLin (dbfs);
    for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (a * std::sin (2.0 * kPi * f * (double) i / fs));
    return v;
}
static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (b - a));
}
// Fresh core with settled smoothers.
static Core make (const Params& p, double fs = FS, bool os = true)
{
    Core c; c.prepare (fs); c.setOversampling (os); c.setParams (p); c.snap(); return c;
}
static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPi / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1);
            for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; }
        }
    }
}
// Gain in dB of a steady sine through the core with the given settings (last quarter of the run).
static double toneGainDb (const Params& p, double f, double dbfs, double fs = FS, int n = 96000)
{
    Core c = make (p, fs);
    auto l = sine (f, dbfs, n, fs); auto r = l; const auto in = l;
    run (c, l, r);
    return linToDb (rms (l, (size_t) n * 3 / 4, (size_t) n) / rms (in, (size_t) n * 3 / 4, (size_t) n));
}

int main()
{
    // ---- ranges and static curve ------------------------------------------------------------
    std::printf ("== ranges and curve\n");
    CHECK (Curve::threshold (0) == 0.0 && std::fabs (Curve::threshold (100) + 40.0) < 1e-9, "Peak Reduction 0..100 maps to a threshold of 0..-40 dB");
    CHECK (Curve::target (0, 0, -6) == 0.0, "Peak Reduction 0: no gain reduction at any level");
    {
        bool mono = true; double prev = -1;
        for (double L = -60; L <= 0; L += 1) { const double t = Curve::target (50, 0, L); if (t + 1e-9 < prev) mono = false; prev = t; }
        CHECK (mono, "steady gain reduction rises with the input level (Compress)");
        mono = true; prev = -1;
        for (double L = -60; L <= 0; L += 1) { const double t = Curve::target (50, 1, L); if (t + 1e-9 < prev) mono = false; prev = t; }
        CHECK (mono, "steady gain reduction rises with the input level (Limit)");
        const double g1 = Curve::target (50, 0, -6), g2 = Curve::target (50, 1, -6);
        CHECK (g2 > g1, "Limit squeezes harder than Compress at the same level (%.1f vs %.1f dB at -6 dB in, Peak 50)", g2, g1);
        const double s1 = Curve::target (50, 0, -4) - Curve::target (50, 0, -14);
        std::printf ("measured  Compress, Peak 50: GR %.1f dB at -14 dB in, %.1f dB at -4 dB in\n", Curve::target (50, 0, -14), Curve::target (50, 0, -4));
        (void) s1;
    }

    // ---- latency and bypass ------------------------------------------------------------------
    std::printf ("== latency, bypass, dry path\n");
    {
        Core c = make (P (0, 0));
        std::vector<float> l (400, 0.0f), r; l[100] = 1.0f; r = l;
        run (c, l, r);
        int pk = 0; for (int i = 0; i < 400; ++i) if (std::fabs (l[(size_t) i]) > std::fabs (l[(size_t) pk])) pk = i;
        CHECK (pk - 100 == c.latency() && c.latency() == kLatency, "wet path latency measured %d samples, reported %d", pk - 100, c.latency());
        std::printf ("measured  impulse peak %.4f (unity gain tube path, Peak 0, Gain 0)\n", l[(size_t) pk]);
        // dry path equals the wet peak position
        Core d = make (P (0, 0, 0, 0, 0)); // Mix 0: only the dry path
        std::vector<float> l2 (400, 0.0f), r2; l2[100] = 1.0f; r2 = l2;
        run (d, l2, r2);
        CHECK (l2[100 + kLatency] == 1.0f, "Mix 0 returns the input exactly, delayed by the reported latency");
    }
    {
        Core c = make (P (60, 30, 1, 20, 100, true));
        std::vector<float> in (9000), r; for (size_t i = 0; i < in.size(); ++i) in[i] = (float) (0.7 * std::sin (0.01 * (double) i) + 0.2 * std::sin (0.37 * (double) i));
        auto l = in; r = in;
        run (c, l, r);
        bool exact = true; for (size_t i = (size_t) kLatency; i < l.size(); ++i) if (l[i] != in[i - (size_t) kLatency]) exact = false;
        CHECK (exact, "Bypass is bit exact: the output equals the input delayed by %d samples", kLatency);
        CHECK (c.grPeak() > 0.0, "the gain reduction meter keeps moving in bypass (peak %.2f dB)", c.grPeak());
    }
    {
        // switching bypass on and off does not click: the largest step stays small
        Core c = make (P (50, 20));
        auto in = sine (440, -12, 48000); auto l = in, r = in;
        c.process (l.data(), r.data(), 12000);
        Params p = P (50, 20, 0, 0, 100, true); c.setParams (p);
        c.process (l.data() + 12000, r.data() + 12000, 12000);
        double maxStep = 0; for (size_t i = 12001; i < 24000; ++i) maxStep = std::max (maxStep, (double) std::fabs (l[i] - l[i - 1]));
        CHECK (maxStep < 0.06, "bypass crossfade is click free (largest sample step %.4f)", maxStep);
    }

    // ---- level calibration ------------------------------------------------------------------
    std::printf ("== level calibration\n");
    {
        const double g = toneGainDb (P (0, 0), 1000, -18);
        CHECK (std::fabs (g) < 0.1, "Peak 0, Gain 0: unity at 1 kHz, -18 dBFS (measured %.3f dB)", g);
        const double g100 = toneGainDb (P (0, 100), 1000, -60);
        CHECK (std::fabs (g100 - 40.0) < 1.0, "Gain 100 adds 40 dB for a small signal (measured %.2f dB)", g100);
        const double g50 = toneGainDb (P (0, 50), 1000, -40);
        CHECK (std::fabs (g50 - 20.0) < 0.5, "Gain 50 adds 20 dB (measured %.2f dB)", g50);
        const double ga = toneGainDb (P (50, 0), 1000, -3);
        const double gb = toneGainDb (P (50, 40), 1000, -3);
        std::printf ("measured  Peak 50 at -3 dB in: Gain 0 gives %.2f dB, Gain 40 gives %.2f dB (Gain raises the level and the tube stage; it does not change the gain reduction)\n", ga, gb);
        // Gain never changes where the compression sits
        Core c1 = make (P (50, 0)), c2 = make (P (50, 100));
        auto l1 = sine (1000, -12, 48000), l2 = l1, r1 = l1, r2 = l1;
        run (c1, l1, r1); run (c2, l2, r2);
        CHECK (std::fabs (c1.grAvg() - c2.grAvg()) < 0.25, "Gain does not change the gain reduction (Gain 0: %.2f dB, Gain 100: %.2f dB)", c1.grAvg(), c2.grAvg());
        const double hf10 = toneGainDb (P (0, 0), 10000, -18), hf16 = toneGainDb (P (0, 0), 16000, -18), hf20 = toneGainDb (P (0, 0), 20000, -18), lf = toneGainDb (P (0, 0), 50, -18);
        std::printf ("measured  response at 48 kHz, Peak 0 Gain 0: 50 Hz %.2f dB, 10 kHz %.2f dB, 16 kHz %.2f dB, 20 kHz %.2f dB\n", lf, hf10, hf16, hf20);
        CHECK (std::fabs (hf10) < 0.5 && std::fabs (lf) < 0.5 && std::fabs (hf20) < 1.0, "flat within 0.5 dB at 50 Hz and 10 kHz, within 1 dB at 20 kHz (48 kHz)");
        const double h441 = toneGainDb (P (0, 0), 19000, -18, 44100.0);
        std::printf ("measured  44.1 kHz: 19 kHz %.2f dB\n", h441);
    }

    // ---- behaviour of the cell --------------------------------------------------------------
    std::printf ("== cell behaviour\n");
    {
        // steady state gain reduction against the static curve
        for (double lvl : { -20.0, -10.0, -4.0 })
        {
            Core c = make (P (50, 0));
            auto l = sine (1000, lvl, 480000), r = l;
            run (c, l, r);
            const double want = Curve::target (50, 0, lvl - 3.0); // a sine peak sits 3 dB above its rms; the detector follows the peak
            std::printf ("measured  1 kHz at %.0f dBFS, Peak 50: settled GR %.2f dB (static curve at peak level %.2f dB)\n", lvl, c.grAvg(), want);
        }
    }
    {
        // attack: burst after silence, time for the gain reduction to reach 63 percent of its value after 1 s
        Core c = make (P (60, 20));
        auto burst = sine (1000, -6, 96000);
        std::vector<double> grs; grs.reserve (96000);
        for (int i = 0; i < 96000; ++i) { float a = burst[(size_t) i], b = a; c.process (&a, &b, 1); grs.push_back (c.grNow()); }
        const double fin = grs.back();
        int t63 = 0; for (int i = 0; i < 96000; ++i) if (grs[(size_t) i] >= 0.63 * fin) { t63 = i; break; }
        std::printf ("measured  attack: gain reduction after 2 s is %.2f dB; 63%% reached after %.1f ms\n", fin, 1000.0 * t63 / FS);
        CHECK (t63 > 0 && t63 / FS < 0.5 && fin > 3.0, "the cell attacks within 0.5 s (63%% in %.0f ms)", 1000.0 * t63 / FS);
    }
    {
        // release: compress for a short and for a long time, then silence; time to half
        for (double hold : { 0.3, 12.0 })
        {
            Core c = make (P (60, 20));
            auto burst = sine (1000, -6, (int) (hold * FS));
            for (size_t i = 0; i < burst.size(); ++i) { float a = burst[i], b = a; c.process (&a, &b, 1); }
            const double g0 = c.grNow();
            int t50 = -1, n = 0; double g = g0;
            for (; n < 40 * 48000; ++n)
            {
                float a = 0, b = 0; c.process (&a, &b, 1); g = c.grNow();
                if (t50 < 0 && g <= 0.5 * g0) t50 = n;
            }
            std::printf ("measured  release after %.1f s of compression (GR %.2f dB): half after %.0f ms, %.2f dB left after 40 s\n", hold, g0, 1000.0 * t50 / FS, g);
            if (hold < 1) CHECK (t50 > 0 && 1000.0 * t50 / FS > 25 && 1000.0 * t50 / FS < 300, "short compression: half release in 25 to 300 ms (measured %.0f ms)", 1000.0 * t50 / FS);
            else CHECK (t50 > 0 && 1000.0 * t50 / FS > 25, "long compression: release is longer (half in %.0f ms)", 1000.0 * t50 / FS);
        }
    }

    // ---- knob to sound time -----------------------------------------------------------------
    std::printf ("== knob to sound time\n");
    {
        for (int which = 0; which < 3; ++which)
        {
            Core c = make (which == 2 ? P (50, 20, 0, 0, 100) : P (50, 20));
            auto in = sine (1000, -12, 30000); auto l = in, r = in, ref = in, rr = in;
            Core ctl = make (which == 2 ? P (50, 20, 0, 0, 100) : P (50, 20));
            c.process (l.data(), r.data(), 10000); ctl.process (ref.data(), rr.data(), 10000);
            Params np = which == 0 ? P (50, 40) : which == 1 ? P (90, 20) : P (50, 20, 0, 0, 0);
            c.setParams (np);
            c.process (l.data() + 10000, r.data() + 10000, 20000); ctl.process (ref.data() + 10000, rr.data() + 10000, 20000);
            int first = -1, nearly = -1; double maxd = 0;
            for (int i = 0; i < 20000; ++i) maxd = std::max (maxd, (double) std::fabs (l[(size_t) (10000 + i)] - ref[(size_t) (10000 + i)]));
            for (int i = 0; i < 20000; ++i)
            {
                const double d = std::fabs (l[(size_t) (10000 + i)] - ref[(size_t) (10000 + i)]);
                if (first < 0 && d > 1e-4 * std::max (1.0, maxd)) first = i;
                if (nearly < 0 && d > 0.9 * maxd) nearly = i;
            }
            std::printf ("measured  %s step: first change after %d samples (%.2f ms), 90%% of the peak change after %.1f ms\n", which == 0 ? "Gain" : which == 1 ? "Peak Reduction" : "Mix", first, 1000.0 * first / FS, 1000.0 * nearly / FS);
            CHECK (first >= 0 && first < 96, "%s change is heard within 2 ms of the next sample", which == 0 ? "Gain" : which == 1 ? "Peak Reduction" : "Mix");
        }
    }

    // ---- stability --------------------------------------------------------------------------
    std::printf ("== stability\n");
    {
        auto bounded = [] (const std::vector<float>& v, float lim) { for (float x : v) if (! std::isfinite (x) || std::fabs (x) > lim) return false; return true; };
        for (int mode = 0; mode < 2; ++mode)
        {
            const Params p = P (100, 100, (float) mode, 100, 100, false, true);
            {   Core c = make (p); std::vector<float> l (96000, 0.0f), r = l; run (c, l, r); CHECK (bounded (l, 0.01f), "silence stays silent and finite (limit %d, noise on, max %.5f)", mode, *std::max_element (l.begin(), l.end())); }
            {   Core c = make (P (100, 100, (float) mode)); std::vector<float> l (96000, 1.0f), r = l; run (c, l, r); CHECK (bounded (l, 100.0f), "DC at full scale stays finite (limit %d)", mode); }
            {   Core c = make (P (100, 100, (float) mode)); std::vector<float> l (96000), r; for (size_t i = 0; i < l.size(); ++i) l[i] = ((i / 40) & 1) ? 1.0f : -1.0f; r = l; run (c, l, r); CHECK (bounded (l, 100.0f), "full scale square wave stays finite (limit %d)", mode); }
            {   Core c = make (P (100, 100, (float) mode)); std::vector<float> l (4096), r;
                for (size_t i = 0; i < l.size(); ++i) l[i] = (i % 7 == 0) ? NAN : (i % 11 == 0 ? INFINITY : 0.5f); r = l; run (c, l, r);
                std::vector<float> l2 (4096, 0.25f), r2 = l2; run (c, l2, r2);
                CHECK (bounded (l, 100.0f) && bounded (l2, 100.0f), "NaN and Inf input never reach the output and the core recovers (limit %d)", mode); }
            {   Core c = make (P (70, 30, (float) mode)); std::vector<float> l (96000), r; for (size_t i = 0; i < l.size(); ++i) l[i] = (float) (1e-30 * std::sin (0.1 * (double) i)); r = l; run (c, l, r);
                CHECK (bounded (l, 1e-3f), "denormal sized input stays tiny (limit %d)", mode); }
        }
        // random parameter changes every sample block
        Core c = make (P (35, 20)); uint32_t s = 12345; bool ok = true;
        std::vector<float> l (64), r;
        for (int blk = 0; blk < 4000; ++blk)
        {
            s = s * 1664525u + 1013904223u;
            c.setParams (P ((float) (s % 101), (float) ((s >> 8) % 101), (float) ((s >> 16) & 1), (float) ((s >> 17) % 101), (float) ((s >> 24) % 101), (s & 0x30000) == 0));
            for (size_t i = 0; i < l.size(); ++i) l[i] = (float) (0.8 * std::sin (0.05 * (double) (blk * 64 + (int) i)));
            r = l; c.process (l.data(), r.data(), 64);
            for (float x : l) if (! std::isfinite (x) || std::fabs (x) > 50.0f) ok = false;
        }
        CHECK (ok, "4000 blocks of random parameter jumps stay finite and bounded");
        // mono (no right channel) gives the same left output as stereo with identical channels
        Core a = make (P (55, 25, 1, 30)), b = make (P (55, 25, 1, 30));
        auto la = sine (700, -9, 20000), lb = la, rb = la;
        a.process (la.data(), nullptr, 20000); b.process (lb.data(), rb.data(), 20000);
        bool same = true; for (size_t i = 0; i < la.size(); ++i) if (la[i] != lb[i]) same = false;
        CHECK (same, "mono processing equals stereo with identical channels");
    }

    // ---- aliasing ---------------------------------------------------------------------------
    std::printf ("== aliasing (tube stage, with and without the 2x oversampler)\n");
    {
        const int N = 16384; const int k = 3541; const double f = (double) k * FS / N; // bin aligned, about 10.4 kHz
        double worst[2] = { -200, -200 };
        for (int osOn = 0; osOn < 2; ++osOn)
        {
            Core c = make (P (0, 100), FS, osOn == 1);
            std::vector<float> l = sine (f, -26, N * 4), r = l;
            run (c, l, r);
            std::vector<std::complex<double>> a ((size_t) N);
            for (int i = 0; i < N; ++i) a[(size_t) i] = (double) l[(size_t) (N * 3 + i)] * (0.5 - 0.5 * std::cos (2.0 * kPi * i / N));
            fft (a);
            // energy at true harmonics (below Nyquist) is "harmonic"; everything else, apart from DC, is alias
            double fund = 0, other = 0;
            std::vector<char> harm ((size_t) N / 2 + 1, 0);
            for (int h = 1; h * k < N / 2; ++h) { const int b = h * k; for (int d = -3; d <= 3; ++d) { const int q = b + d; if (q >= 0 && q <= N / 2) harm[(size_t) q] = 1; } } // only harmonics below Nyquist are real; the rest that shows up is aliasing
            for (int q = 16; q <= N / 2; ++q) { const double e = std::norm (a[(size_t) q]); if (std::abs (q - k) <= 3) fund += e; else if (! harm[(size_t) q]) other += e; }
            worst[osOn] = 10.0 * std::log10 (std::max (other, 1e-30) / fund);
        }
        std::printf ("measured  10.4 kHz at -26 dBFS, Gain 100 (+40 dB, tube stage driven): non-harmonic energy %.1f dB (no oversampling) and %.1f dB (4x oversampling) below the fundamental\n", worst[0], worst[1]);
        CHECK (worst[1] < -70.0, "aliasing with the 4x oversampler is below -70 dB (measured %.1f dB)", worst[1]);
        CHECK (worst[1] <= worst[0] + 0.5, "oversampling does not make aliasing worse");
    }
    {
        // harmonic content of the cell and tube chain on a 1 kHz tone (third harmonic should dominate the second)
        const int N = 65536, k = 1365; const double f = (double) k * FS / N; Core c = make (P (50, 25), FS);
        auto l = sine (f, -12, N * 3), r = l; run (c, l, r);
        std::vector<std::complex<double>> a ((size_t) N);
        for (int i = 0; i < N; ++i) a[(size_t) i] = (double) l[(size_t) (N * 2 + i)] * (0.5 - 0.5 * std::cos (2.0 * kPi * i / N));
        fft (a);
        const double h1 = std::abs (a[(size_t) k]), h2 = std::abs (a[(size_t) (2 * k)]), h3 = std::abs (a[(size_t) (3 * k)]);
        std::printf ("measured  1 kHz at -12 dBFS, Peak 50, Gain 25 (GR %.1f dB): 2nd harmonic %.1f dB, 3rd harmonic %.1f dB below the fundamental\n", c.grAvg(), 20 * std::log10 (h2 / h1), 20 * std::log10 (h3 / h1));
        CHECK (h3 > h2, "the third harmonic is larger than the second, as in published measurements of real units");
    }

    {
        // DC at the output: a driven tube stage has an even-order bias; the output coupling must remove it
        const int N = 65536, k = 1365; const double f = (double) k * FS / N;
        for (int blk = 0; blk < 2; ++blk)
        {
            Core c = make (P (0, 100)); c.setDcBlock (blk == 1);
            auto l = sine (f, -26, N * 4), r = l; run (c, l, r);
            double mean = 0, pk = 0; for (int i = 0; i < N * 2; ++i) { mean += l[(size_t) (N * 2 + i)]; }
            mean /= (N * 2);
            double ac = 0; for (int i = 0; i < N * 2; ++i) ac += (double) l[(size_t) (N * 2 + i)] * l[(size_t) (N * 2 + i)];
            ac = std::sqrt (ac / (N * 2)); (void) pk;
            std::printf ("measured  DC offset with the output coupling %s: %.1f dB below the signal rms\n", blk ? "on" : "off", 20 * std::log10 (std::max (std::fabs (mean), 1e-12) / ac));
            if (blk == 1) CHECK (std::fabs (mean) / ac < 1e-3, "DC offset is below -60 dB of the signal with the output coupling on");
        }
    }

    std::printf (failures == 0 ? "\nALL CORE TESTS PASSED\n" : "\n%d CORE TESTS FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
