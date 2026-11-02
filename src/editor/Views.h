#pragma once
#include "Widgets.h"
#include "../core/LapCat2A.h"

namespace ui
{
// Everything the views draw, refreshed by the editor timer (message thread only).
struct Live
{
    double fs = 44100.0;
    float inPk = 0, outPk = 0;       // linear, decaying peak hold
    float grNow = 0;                 // instantaneous gain reduction, dB, positive
    float detIn = -90;               // detector input level, dB
    float peak = 35, emph = 0;       // parameters
    int mode = 0, meter = 0;
    bool bypass = false;
    float needle = 0, nvel = 0;      // meter needle position in meter units (dB)
    float hist[360] {}; int hi = 0;
};

inline float l2d (float x) { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }
inline float d2l (float db) { return std::pow (10.0f, db * 0.05f); }

// Base of the display canvases: drawn in the mockup's canvas units, scaled into a bordered box.
class CanvasView : public Component, public TipClient
{
public:
    CanvasView (const Ctx& c, const Live& l, float cw, float ch) : ctx (c), live (l), cvW (cw), cvH (ch) {}
    std::function<void (float cx, float cy, bool first)> onDragAt; // canvas units
    std::function<void()> onDragEnd;
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat();
        g.setColour (t.disp);
        g.fillRoundedRectangle (r, 3.0f);
        auto inner = r.reduced (1.0f);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (inner.toNearestInt());
            g.addTransform (AffineTransform::scale (inner.getWidth() / cvW, inner.getHeight() / cvH).translated (inner.getX(), inner.getY()));
            draw (g, t, cvW, cvH);
        }
        g.setColour (t.line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
    void mouseDown (const MouseEvent& e) override { if (onDragAt) onDragAt (toCx (e), toCy (e), true); }
    void mouseDrag (const MouseEvent& e) override { if (onDragAt) onDragAt (toCx (e), toCy (e), false); }
    void mouseUp (const MouseEvent&) override { if (onDragEnd) onDragEnd(); }
protected:
    float toCx (const MouseEvent& e) const { return (float) e.x / (float) getWidth() * cvW; }
    float toCy (const MouseEvent& e) const { return (float) e.y / (float) getHeight() * cvH; }
    virtual void draw (Graphics&, const theme::Look&, float w, float h) = 0;
    static void label (Graphics& g, const String& s, float x, float y, Justification j, Colour c)
    {
        g.setFont (theme::font (theme::FontKind::mono, 9.0f));
        g.setColour (c);
        g.drawSingleLineText (s, roundToInt (x), roundToInt (y), j);
    }
    static void dashedLine (Graphics& g, Line<float> ln, float a, float b, float w)
    {
        const float d[2] = { a, b };
        g.drawDashedLine (ln, d, 2, w);
    }
    const Ctx& ctx;
    const Live& live;
    float cvW, cvH;
};

// Transfer curve: output against input level, both before Gain. Drag sideways to move Peak Reduction.
class CurveView : public CanvasView
{
public:
    CurveView (const Ctx& c, const Live& l) : CanvasView (c, l, 282, 112) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float l = 24, b = 12, pw = w - l - 6, ph = h - b - 6;
        auto X = [=] (float d) { return l + (jlimit (-60.0f, 0.0f, d) + 60.0f) / 60.0f * pw; };
        auto Y = [=] (float d) { return 6.0f + (1.0f - (jlimit (-60.0f, 0.0f, d) + 60.0f) / 60.0f) * ph; };
        for (float d : { -48.0f, -36.0f, -24.0f, -12.0f, 0.0f })
        {
            g.setColour (t.line);
            g.drawLine (X (d), 6.0f, X (d), 6.0f + ph, 1.0f);
            g.drawLine (l, Y (d), l + pw, Y (d), 1.0f);
            label (g, String ((int) d), l - 3.0f, Y (d) + 3.0f, Justification::right, t.dim);
            label (g, String ((int) d), X (d) + 8.0f, h - 2.0f, Justification::right, t.dim);
        }
        g.setColour (Colour (0xff565a63));
        dashedLine (g, { X (-60), Y (-60), X (0), Y (0) }, 3, 3, 1.0f);
        const float thr = (float) lap::Curve::threshold (live.peak);
        if (live.peak >= 0.5f)
        {
            g.setColour (Colour (0xffFFD100));
            dashedLine (g, { X (thr), 6.0f, X (thr), 6.0f + ph }, 2, 3, 1.0f);
        }
        Path curve;
        for (int d = -60; d <= 0; ++d)
        {
            const float o = (float) d - (float) lap::Curve::target (live.peak, live.mode ? 1.0 : 0.0, d);
            if (d == -60) curve.startNewSubPath (X ((float) d), Y (o)); else curve.lineTo (X ((float) d), Y (o));
        }
        g.setColour (t.accent);
        g.strokePath (curve, PathStrokeType (2.0f));
        const float li = live.detIn, lo = li - live.grNow;
        if (li > -60.0f)
        {
            g.setColour (theme::red);
            g.fillEllipse (X (li) - 3.5f, Y (lo) - 3.5f, 7.0f, 7.0f);
        }
    }
};

// Gain reduction over the last six seconds.
class HistView : public CanvasView
{
public:
    HistView (const Ctx& c, const Live& l) : CanvasView (c, l, 282, 112) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float l = 26, b = 12, pw = w - l - 6, ph = h - b - 6;
        auto Y = [=] (float d) { return 6.0f + jlimit (0.0f, 20.0f, d) / 20.0f * ph; };
        for (int d : { 0, 5, 10, 15, 20 })
        {
            g.setColour (t.line);
            g.drawLine (l, Y ((float) d), l + pw, Y ((float) d), 1.0f);
            label (g, d == 0 ? String ("0") : "-" + String (d), l - 3.0f, Y ((float) d) + 3.0f, Justification::right, t.dim);
        }
        for (int s : { 0, 2, 4, 6 })
        {
            const float x = l + pw * ((float) s / 6.0f);
            g.setColour (t.line);
            g.drawLine (x, 6.0f, x, 6.0f + ph, 1.0f);
            label (g, s == 6 ? String ("now") : "-" + String (6 - s) + "s", jlimit (l + 10.0f, w - 2.0f, x + (s == 6 ? 0.0f : 8.0f)), h - 2.0f, Justification::right, t.dim);
        }
        Path line;
        for (int i = 0; i < 360; ++i)
        {
            const float v = live.hist[(live.hi + i) % 360], x = l + pw * (float) i / 359.0f;
            if (i == 0) line.startNewSubPath (x, Y (v)); else line.lineTo (x, Y (v));
        }
        Path fill (line);
        fill.lineTo (l + pw, Y (0)); fill.lineTo (l, Y (0)); fill.closeSubPath();
        g.setColour (theme::pink.withAlpha (0.35f)); g.fillPath (fill);
        g.setColour (theme::pink); g.strokePath (line, PathStrokeType (1.6f));
    }
};

// What the detector listens to. Drag up or down to move HF Emphasis.
class FiltView : public CanvasView
{
public:
    FiltView (const Ctx& c, const Live& l) : CanvasView (c, l, 282, 112) {}
    static float filtDb (float emph, float f)
    {
        const float k = 0.85f * emph / 100.0f, x = f / (float) lap::kEmphFc, re = 1.0f / (1.0f + x * x), im = -x / (1.0f + x * x);
        return 20.0f * std::log10 (std::hypot (1.0f - k * re, k * im));
    }
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float l = 26, b = 12, pw = w - l - 6, ph = h - b - 6;
        auto X = [=] (float f) { return l + std::log (f / 20.0f) / std::log (1000.0f) * pw; };
        auto Y = [=] (float d) { return 6.0f + (1.0f - (jlimit (-18.0f, 3.0f, d) + 18.0f) / 21.0f) * ph; };
        for (int d : { 0, -6, -12, -18 })
        {
            g.setColour (t.line);
            g.drawLine (l, Y ((float) d), l + pw, Y ((float) d), 1.0f);
            label (g, String (d), l - 3.0f, Y ((float) d) + 3.0f, Justification::right, t.dim);
        }
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            g.setColour (t.line);
            g.drawLine (X (f), 6.0f, X (f), 6.0f + ph, 1.0f);
            label (g, f >= 1000.0f ? String ((int) (f / 1000.0f)) + "k" : String ((int) f), X (f) + 8.0f, h - 2.0f, Justification::right, t.dim);
        }
        g.setColour (Colour (0xff565a63));
        dashedLine (g, { l, Y (0), l + pw, Y (0) }, 3, 3, 1.0f);
        Path line;
        for (int i = 0; i <= 120; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 120.0f);
            if (i == 0) line.startNewSubPath (X (f), Y (filtDb (live.emph, f))); else line.lineTo (X (f), Y (filtDb (live.emph, f)));
        }
        Path fill (line);
        fill.lineTo (X (20000), Y (-18)); fill.lineTo (X (20), Y (-18)); fill.closeSubPath();
        g.setColour (theme::blue.withAlpha (0.12f)); g.fillPath (fill);
        g.setColour (theme::blue); g.strokePath (line, PathStrokeType (2.0f));
    }
};

// In and Out level strips, -60 to +6 dB, neon blue, purple and pink. Click to clear the clip marker.
class MeterView : public Component, public TipClient
{
public:
    explicit MeterView (const Ctx& c) : ctx (c) {}
    float lin = 0.0f;
    bool latch = false;
    void update (float l)
    {
        const bool was = latch;
        const float oldDb = l2d (lin);
        lin = l;
        if (l2d (l) >= 0.0f) latch = true;
        if (was != latch || std::abs (l2d (l) - oldDb) > 0.05f) repaint();
    }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const float w = (float) getWidth(), h = (float) getHeight(), mn = -60, mx = 6;
        auto y = [=] (float db) { return h - 6.0f - (jlimit (mn, mx, db) - mn) / (mx - mn) * (h - 12.0f); };
        g.fillAll (t.disp);
        const float db = l2d (lin), top = y (db);
        auto f = [=] (float d) { return (d - mn) / (mx - mn); };
        ColourGradient grad (theme::blue, 0.0f, y (mn), theme::red, 0.0f, y (mx), false);
        grad.addColour (f (-12.0f), theme::blue);
        grad.addColour (f (-12.0f) + 0.001, theme::purple);
        grad.addColour (f (0.0f), theme::purple);
        grad.addColour (f (0.0f) + 0.001, theme::red);
        g.setGradientFill (grad);
        g.fillRect (2.0f, top, 12.0f, y (mn) - top);
        g.setFont (theme::font (theme::FontKind::mono, 9.0f));
        for (float tk : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f })
        {
            const float yy = y (tk);
            g.setColour (t.dim);
            g.fillRect (14.0f, yy, 3.0f, 1.0f);
            g.drawSingleLineText (String ((int) tk), roundToInt (w - 1.0f), roundToInt (yy + 3.0f), Justification::right);
        }
        g.setColour (latch ? theme::red : Colour (0xff2a2c31));
        g.fillRect (2.0f, 0.0f, 12.0f, 4.0f);
    }
    void mouseDown (const MouseEvent&) override { latch = false; repaint(); }
private:
    const Ctx& ctx;
};

// The VU style meter face with the swinging needle. Canvas units 440 x 200 as in the mockup.
class VUView : public Component, public TipClient
{
public:
    VUView (const Ctx& c, const Live& l) : ctx (c), live (l) {}
    static float vuFrac (float db) { return jlimit (0.0f, 1.0f, (std::pow (10.0f, db / 20.0f) - 0.1f) / (1.4125f - 0.1f)); }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const float W = 440.0f, H = 200.0f;
        {
            Path clip; clip.addRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
            g.reduceClipRegion (clip);
        }
        g.addTransform (AffineTransform::scale ((float) getWidth() / W, (float) getHeight() / H));
        ColourGradient bg (Colour (0xfffff8e6), 0.0f, 0.0f, Colour (0xffcbbd98), 0.0f, H, false);
        bg.addColour (0.5, t.face);
        g.setGradientFill (bg); g.fillRect (0.0f, 0.0f, W, H);
        {
            ColourGradient gl (Colour::fromFloatRGBA (1.0f, 200.0f / 255.0f, 90.0f / 255.0f, 0.35f), W / 2, H * 0.5f,
                               Colour::fromFloatRGBA (1.0f, 200.0f / 255.0f, 90.0f / 255.0f, 0.0f), W / 2 + 230.0f, H * 0.5f, true);
            gl.clearColours();
            gl.addColour (0.0, Colour::fromFloatRGBA (1.0f, 200.0f / 255.0f, 90.0f / 255.0f, 0.35f));
            gl.addColour (10.0 / 230.0, Colour::fromFloatRGBA (1.0f, 200.0f / 255.0f, 90.0f / 255.0f, 0.35f));
            gl.addColour (1.0, Colour::fromFloatRGBA (1.0f, 200.0f / 255.0f, 90.0f / 255.0f, 0.0f));
            g.setGradientFill (gl); g.fillRect (0.0f, 0.0f, W, H);
        }
        const float px = W / 2, py = 262.0f, R = 200.0f, A = 44.0f;
        auto ang = [=] (float f) { return degreesToRadians (-A + 2 * A * f); };
        auto pt = [=] (float f, float r) { return Point<float> (px + std::sin (ang (f)) * r, py - std::cos (ang (f)) * r); };
        const bool gr = live.meter == 0;
        const Colour redTick (0xffb3262f), ink = theme::faceInk;
        {
            Path arc;
            for (int i = 0; i <= 60; ++i) { auto a = pt ((float) i / 60.0f, R); if (i) arc.lineTo (a); else arc.startNewSubPath (a); }
            g.setColour (ink); g.strokePath (arc, PathStrokeType (1.6f));
        }
        struct Tk { float d, f; bool lab; };
        std::vector<Tk> ticks;
        float val;
        {
            Path red;
            if (gr)
            {
                for (int d = 0; d <= 20; d += 2) ticks.push_back ({ (float) d, 1.0f - (float) d / 20.0f, true });
                for (int d = 1; d < 20; d += 2) ticks.push_back ({ (float) d, 1.0f - (float) d / 20.0f, false });
                val = 1.0f - jlimit (0.0f, 20.0f, live.needle) / 20.0f;
                for (int i = 0; i <= 12; ++i) { auto a = pt ((float) i / 12.0f * 0.2f, R - 5.0f); if (i) red.lineTo (a); else red.startNewSubPath (a); }
            }
            else
            {
                for (float d : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
                    ticks.push_back ({ d, vuFrac (d), d == -20 || d == -10 || d == -7 || d == -5 || d == -3 || d == 0 || d == 3 });
                val = vuFrac (live.needle);
                for (int i = 0; i <= 12; ++i) { auto a = pt (vuFrac (0) + (1 - vuFrac (0)) * (float) i / 12.0f, R - 5.0f); if (i) red.lineTo (a); else red.startNewSubPath (a); }
            }
            g.setColour (redTick); g.strokePath (red, PathStrokeType (3.0f));
        }
        g.setFont (theme::font (theme::FontKind::mono, 13.0f));
        for (auto& tk : ticks)
        {
            auto a = pt (tk.f, R), b = pt (tk.f, R - (tk.lab ? 15.0f : 9.0f));
            g.setColour ((gr ? tk.d <= 0 : tk.d >= 0) ? redTick : ink);
            g.drawLine (a.x, a.y, b.x, b.y, tk.lab ? 2.0f : 1.2f);
            if (tk.lab)
            {
                auto tp = pt (tk.f, R - 31.0f);
                g.setColour ((! gr && tk.d >= 0) ? redTick : ink);
                const String s = gr ? String ((int) tk.d) : (tk.d > 0 ? "+" + String ((int) tk.d) : String ((int) tk.d));
                g.drawSingleLineText (s, roundToInt (tp.x), roundToInt (tp.y + 4.0f), Justification::horizontallyCentred);
            }
        }
        g.setColour (ink);
        g.setFont (theme::font (theme::FontKind::brand, 17.0f));
        g.drawSingleLineText (gr ? "GAIN REDUCTION dB" : "OUTPUT VU", roundToInt (W / 2), 142, Justification::horizontallyCentred);
        g.setFont (theme::font (theme::FontKind::semi, 12.0f));
        g.drawSingleLineText (gr ? "0 = no compression" : (live.meter == 1 ? "0 VU = +10 level" : "0 VU = +4 level"), roundToInt (W / 2), 158, Justification::horizontallyCentred);
        g.setFont (theme::font (theme::FontKind::semi, 11.0f));
        g.setColour (Colour::fromFloatRGBA (42.0f / 255.0f, 36.0f / 255.0f, 24.0f / 255.0f, 0.7f));
        g.drawSingleLineText ("OPTICAL LEVELER", roundToInt (W / 2), 172, Justification::horizontallyCentred);
        const float vf = jlimit (0.0f, 1.0f, val);
        const auto tip = pt (vf, R - 6.0f), base = pt (vf, 40.0f);
        g.setColour (Colours::black.withAlpha (0.3f));
        g.drawLine (base.x + 2, base.y + 2, tip.x + 2, tip.y + 2, 3.0f);
        g.setColour (Colour (0xff8a1620));
        g.drawLine (px, py, tip.x, tip.y, 2.4f);
        g.setColour (Colour (0xff16171a)); g.fillEllipse (px - 56.0f, H + 34.0f - 56.0f, 112.0f, 112.0f);
        g.setColour (theme::pink);         g.fillEllipse (px - 10.0f, H + 34.0f - 10.0f, 20.0f, 20.0f);
        if (live.bypass)
        {
            g.setColour (Colour::fromFloatRGBA (22.0f / 255.0f, 23.0f / 255.0f, 26.0f / 255.0f, 0.55f)); g.fillRect (0.0f, 0.0f, W, H);
            g.setColour (Colours::white); g.setFont (theme::font (theme::FontKind::bold, 18.0f));
            g.drawSingleLineText ("BYPASSED", roundToInt (W / 2), 60, Justification::horizontallyCentred);
        }
    }
private:
    const Ctx& ctx;
    const Live& live;
};
} // namespace ui
