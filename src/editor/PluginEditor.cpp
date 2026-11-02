#include "PluginEditor.h"
#include "Widgets.h"
#include "Views.h"
#include "Dialogs.h"
#include "Presets.h"

using namespace juce;
using namespace ui;

namespace
{
// ---- the wood cheeks: same three colours and vertical grain as PurrOne ---------------------------
struct Mul32
{
    uint32_t a;
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = a ^ (a >> 15);
        t *= (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

Image makeWood (uint32_t seed, bool edgeRight)
{
    const int w = 104, h = 1280;
    Image img (Image::ARGB, w, h, true);
    Graphics g (img);
    Mul32 r { seed };
    g.fillAll (Colour (0xff3d2210));
    for (int x = 0; x < w; ++x)
    {
        const double a = r.next();
        g.setColour (Colour::fromFloatRGBA (123 / 255.0f, 74 / 255.0f, 38 / 255.0f, (float) (0.40 * (0.4 + 0.6 * a))));
        g.fillRect (x, 0, 1, h);
        if (r.next() < 0.35)
        {
            const float al = (float) (0.75 * r.next());
            const double y0 = r.next() * h;
            const double len = h * (0.2 + 0.6 * r.next());
            g.setColour (Colour::fromFloatRGBA (90 / 255.0f, 52 / 255.0f, 25 / 255.0f, al));
            g.fillRect ((float) x, (float) y0, 1.0f, (float) len);
        }
        if (r.next() < 0.12)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRect (x, 0, 1, h);
        }
    }
    g.setColour (Colours::black);
    g.fillRect (edgeRight ? 0 : w - 3, 0, 3, h);
    return img;
}

std::unique_ptr<Drawable> makeLogo()
{
    static const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64"><path d="M8 6 L24 22 H40 L56 6 L60 36 Q60 58 32 60 Q4 58 4 36 Z" fill="#FF7BB1"/><path d="M12 14 L21 23 H14 Z M52 14 L43 23 H50 Z" fill="#FBBDD0"/><ellipse cx="22" cy="36" rx="4" ry="5" fill="#16171a"/><ellipse cx="42" cy="36" rx="4" ry="5" fill="#16171a"/><path d="M29 44 H35 L32 48 Z" fill="#16171a"/><path d="M32 48 Q28 53 24 50 M32 48 Q36 53 40 50" stroke="#16171a" stroke-width="2" fill="none"/><path d="M2 40 H14 M2 46 H14 M62 40 H50 M62 46 H50" stroke="#FBBDD0" stroke-width="1.6"/></svg>)";
    if (auto xml = XmlDocument::parse (svg)) return Drawable::createFromSVG (*xml);
    return {};
}

class Cheek : public Component, public TipClient
{
public:
    Cheek (Image i, std::function<void()> reset) : img (std::move (i)), onReset (std::move (reset)) { setMouseCursor (MouseCursor::NormalCursor); }
    void paint (Graphics& g) override
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, getLocalBounds().toFloat());
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onReset) onReset(); }
private:
    Image img;
    std::function<void()> onReset;
};

struct Delay : Timer
{
    std::function<void()> fn;
    void start (int ms) { startTimer (ms); }
    void timerCallback() override { stopTimer(); if (fn) fn(); }
};

class Bubble : public Component
{
public:
    Bubble (const Ctx& c) : ctx (c) { setInterceptsMouseClicks (false, false); setVisible (false); }
    void showAt (const String& s, Rectangle<int> target, int parentW, int parentH)
    {
        text = s;
        AttributedString as = build();
        TextLayout tl; tl.createLayout (as, 282.0f);
        const int tw = (int) std::ceil (std::min (282.0f, tl.getWidth())) + 20, th = (int) std::ceil (tl.getHeight()) + 14;
        int x = jlimit (60, std::max (60, parentW - tw - 8), target.getX());
        int y = target.getBottom() + 6;
        if (y + th > parentH - 4) y = target.getY() - th - 6;
        setBounds (x, y, tw, th);
        setVisible (true);
        toFront (false);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        build().draw (g, r.reduced (10.0f, 7.0f));
    }
private:
    AttributedString build() const
    {
        AttributedString as; as.setWordWrap (AttributedString::byWord); as.setLineSpacing (2.0f);
        as.append (text, theme::font (theme::FontKind::regular, 14.0f), ctx.t().ink);
        return as;
    }
    const Ctx& ctx;
    String text;
};

class Toast : public Component
{
public:
    Toast (const Ctx&) { setInterceptsMouseClicks (false, false); setVisible (false); done.fn = [this] { setVisible (false); }; }
    void say (const String& s, int parentW, int parentH)
    {
        text = s;
        const int w = (int) textWidth (theme::font (theme::FontKind::regular, 15.0f), s) + 30;
        setBounds ((parentW - w) / 2, parentH - 34 - 28, w, 28);
        setVisible (true); toFront (false); repaint();
        done.start (1600);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        drawText (g, text, r, theme::font (theme::FontKind::regular, 15.0f), Colours::white);
    }
private:
    String text;
    Delay done;
};


// A rectangle that only carries an info bubble (the peak readouts).
class TipArea : public Component, public TipClient {};
} // namespace

// =====================================================================================================
class Face : public Component, public DialogHost, private Timer
{
public:
    Face (LapCat2AProcessor& p, std::function<void()> resetSize)
        : proc (p), onResetSize (std::move (resetSize)), lnf (ctx), logo (makeLogo()),
          vu (ctx, live), curve (ctx, live), filt (ctx, live), hist (ctx, live),
          meterIn (ctx), meterOut (ctx), bubble (ctx), toastView (ctx), modal (*this)
    {
        const String lk = proc.getSetting (setting::look, "C").toString();
        ctx.look = lk == "A" ? 'A' : lk == "B" ? 'B' : 'C';
        ctx.tips = (bool) proc.getSetting (setting::tips, true);
        ctx.power = (bool) proc.getSetting (setting::power, false);
        setLookAndFeel (&lnf);
        lnf.refresh();
        setSize (layout::W, layout::H);
        setWantsKeyboardFocus (true);

        addAndMakeVisible (cheekL = new Cheek (makeWood (11, true), onResetSize));
        addAndMakeVisible (cheekR = new Cheek (makeWood (29, false), onResetSize));
        place (*cheekL, layout::cheekL); place (*cheekR, layout::cheekR);
        cheekL->tip = cheekR->tip = layout::cheekTip;

        buildHeader();
        buildHardware();
        buildDisplays();
        buildMeters();

        addAndMakeVisible (bubble);
        addAndMakeVisible (toastView);
        addChildComponent (modal);
        addMouseListener (this, true);
        tipTimer.fn = [this] { showTip(); };
        syncName();
        refreshLive (1.0f / 60.0f);
        startTimerHz (ctx.power ? 16 : 60);
    }
    ~Face() override { stopTimer(); removeMouseListener (this); setLookAndFeel (nullptr); }

    void openForTesting (const String& n)
    {
        forceLive = true; // the snapshot tool has no desktop window, so isShowing() would be false
        if (n == "settings") modal.openSettings();
        else if (n == "guide") modal.openGuide (0);
        else if (n == "guide2") modal.openGuide (1);
        else if (n == "guide3") modal.openGuide (2);
        else if (n == "about") modal.openAbout();
        else if (n == "presets") modal.openBrowser();
        else if (n == "save") modal.openSave (nullptr);
        else if (n == "export") modal.openExport (nullptr);
        else if (n == "import") modal.openImport();
        else if (n == "live") {}
        else if (n == "tip") { tipHost = &meterIn; tipText = "Info bubble check."; showTip(); }
    }
    void setForceLive (bool b) { forceLive = b; }

    // ---- DialogHost --------------------------------------------------------------------------
    LapCat2AProcessor& processor() override { return proc; }
    presets::Store& store() override { return presetStore; }
    Ctx& context() override { return ctx; }
    String presetName() override { return proc.getSetting (setting::preset, presets::factory()[0].name).toString(); }
    void setPresetName (const String& n) override { proc.setSetting (setting::preset, n); syncName(); }
    void toast (const String& s) override { toastView.say (s, getWidth(), getHeight()); }
    void resetWindow() override { if (onResetSize) onResetSize(); }
    void settingsChanged() override
    {
        lnf.refresh();
        startTimerHz (ctx.power ? 16 : 60);
        repaint();
        repaintAll (*this);
    }

    Preset currentValues() override
    {
        Preset p;
        p.v[0] = real (ids::peak); p.v[1] = real (ids::gain); p.v[2] = real (ids::mode); p.v[3] = real (ids::meter);
        p.v[4] = real (ids::emph); p.v[5] = real (ids::mix); p.v[6] = real (ids::bypass);
        for (int i : { 0, 1, 4, 5 }) p.v[i] = std::round (p.v[i] * 1000.0) / 1000.0;
        return p;
    }

    void loadPreset (const Preset& p) override
    {
        setReal (ids::peak, (float) p.v[0]); setReal (ids::gain, (float) p.v[1]); setReal (ids::mode, (float) p.v[2]);
        setReal (ids::emph, (float) p.v[4]); setReal (ids::mix, (float) p.v[5]);
        setPresetName (p.name);
        toast ("Loaded " + p.name);
        refreshLive (1.0f / 60.0f);
    }

    bool dirty() override
    {
        const Preset* base = presetStore.find (presetName());
        if (base == nullptr) base = &presets::factory()[0];
        const Preset now = currentValues();
        for (int i : { 0, 1, 2, 4, 5 })
            if (std::abs (now.v[i] - base->v[i]) > 0.05) return true;
        return false;
    }

    // ---- painting ----------------------------------------------------------------------------
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        g.fillAll (Colours::black);
        g.setColour (t.plate); g.fillRect (rc (layout::mainArea));
        // header
        g.setColour (t.plate2); g.fillRect (rc (layout::hdr));
        g.setColour (t.line); g.fillRect (layout::hdr.x, layout::hdr.h - 1.0f, layout::hdr.w, 1.0f);
        if (logo) logo->drawWithin (g, rc (layout::logo), RectanglePlacement::centred, 1.0f);
        drawText (g, "LapCat", rc (layout::name).withWidth (layout::badge.x - layout::name.x - 3.0f), theme::font (theme::FontKind::brand, 26.0f), Colours::white, Justification::centredLeft, 0.3f);
        g.setColour (theme::amber); g.fillRoundedRectangle (rc (layout::badge), 4.0f);
        drawText (g, "2A", rc (layout::badge).translated (0.0f, 0.5f), theme::font (theme::FontKind::bold, 17.0f), Colour (0xff1b1206), Justification::centred, 1.0f);
        drawText (g, "STUDIO 87", rc (layout::s87), theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centred, 2.0f);
        // hardware row
        {
            ColourGradient gr (t.plate, 0.0f, layout::hw.y, t.plate2, 0.0f, layout::hw.y + layout::hw.h, false);
            g.setGradientFill (gr); g.fillRect (rc (layout::hw));
            g.setColour (t.line); g.fillRect (layout::hw.x, layout::hw.y + layout::hw.h - 1.0f, layout::hw.w, 1.0f);
        }
        g.setColour (t.plate2); g.fillRect (rc (layout::disp));
        g.setColour (t.line); g.fillRect (layout::disp.x, layout::disp.y + layout::disp.h - 1.0f, layout::disp.w, 1.0f);
        drawVuFrame (g);
        for (int i = 0; i < 4; ++i)
        {
            auto& k = layout::knobs[i];
            const bool big = k.big;
            drawText (g, String (k.labText).toUpperCase(), rc (k.lab), theme::font (theme::FontKind::bold, big ? 15.0f : 14.0f), t.title,
                      big ? Justification::centred : Justification::centredLeft, big ? 3.0f : 2.0f);
        }
        for (auto& c : layout::swCaps)
            drawText (g, String (c.text).toUpperCase(), rc (c.r), theme::font (theme::FontKind::regular, 12.0f), t.title, Justification::centred, 2.0f);
        drawSeg (g, layout::segMeter);
        // displays
        for (auto& c : layout::dispCaps)
            drawText (g, String (c.text).toUpperCase(), rc (c.r), theme::font (theme::FontKind::regular, 12.0f), t.title, Justification::centredLeft, 1.5f);
        drawSeg (g, layout::segMode);
        // meter column
        g.setColour (t.plate2); g.fillRect (rc (layout::meters));
        g.setColour (t.line); g.fillRect (layout::meters.x, layout::meters.y, 1.0f, layout::meters.h);
        for (auto& c : layout::meterCaps)
            drawText (g, String (c.text).toUpperCase(), rc (c.r), theme::font (theme::FontKind::regular, 12.0f), t.title, Justification::centred, 1.5f);
        {
            auto pk = [] (float lin) { return lin > 1.0e-4f ? String (l2d (lin), 1) : String ("-inf"); };
            const auto f = theme::font (theme::FontKind::mono, 11.0f);
            const String a = pk (live.inPk), b = pk (live.outPk);
            const float wa = textWidth (f, a), wb = textWidth (f, b);
            const float cx = (layout::pkIn.x + layout::pkOut.x + layout::pkOut.w) * 0.5f;
            const float x0 = cx - (wa + 6.0f + wb) * 0.5f;
            drawText (g, a, { x0, layout::pkIn.y, wa, layout::pkIn.h }, f, t.dim);
            drawText (g, b, { x0 + wa + 6.0f, layout::pkIn.y, wb, layout::pkIn.h }, f, t.dim);
        }
        // footer
        g.setColour (t.plate2); g.fillRect (rc (layout::foot));
        g.setColour (t.line); g.fillRect (layout::foot.x, layout::foot.y, layout::foot.w, 1.0f);
        drawText (g, String ("Studio 87 ") + String::fromUTF8 ("\xc2\xb7") + " LapCat2A v" + JucePlugin_VersionString,
                  { layout::footLeft.x, layout::footLeft.y, 400.0f, layout::footLeft.h }, theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredLeft, 1.0f);
        if (auto* ed = findParentComponentOfClass<LapCat2AEditor>())
            drawText (g, String (ed->getWidth()) + String::fromUTF8 (" \xc3\x97 ") + String (ed->getHeight()),
                      { layout::winsz.x + layout::winsz.w - 140.0f, layout::winsz.y, 140.0f, layout::winsz.h },
                      theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredRight, 1.0f);
    }

    // ---- keyboard ----------------------------------------------------------------------------
    bool keyPressed (const KeyPress& k) override
    {
        if (modal.isOpen() || k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown() || k.getModifiers().isAltDown()) return false;
        const juce_wchar c = CharacterFunctions::toLowerCase (k.getTextCharacter());
        if (c == 'b') toggle (ids::bypass);
        else if (c == 'c') setReal (ids::mode, real (ids::mode) > 0.5f ? 0.0f : 1.0f);
        else if (c == 'm') setReal (ids::meter, (float) (((int) real (ids::meter) + 1) % 3));
        else return false;
        syncControls();
        return true;
    }

    // ---- info bubbles ------------------------------------------------------------------------
    void mouseMove (const MouseEvent& e) override { trackTip (e); }
    void mouseEnter (const MouseEvent& e) override { trackTip (e); }
    void mouseExit (const MouseEvent&) override { hideTip(); }
    void mouseDown (const MouseEvent& e) override
    {
        hideTip();
        if (dynamic_cast<TextEditor*> (e.eventComponent) == nullptr && ! modal.isOpen()) grabKeyboardFocus();
    }

private:
    static void repaintAll (Component& c) { for (auto* ch : c.getChildren()) { ch->repaint(); repaintAll (*ch); } }
    static void place (Component& c, const layout::Rc& r) { c.setBounds (roundToInt (r.x), roundToInt (r.y), roundToInt (r.w), roundToInt (r.h)); }

    void drawSeg (Graphics& g, const layout::Rc& r)
    {
        auto& t = ctx.t();
        g.setColour (t.disp); g.fillRoundedRectangle (rc (r), 5.0f);
        g.setColour (t.line); g.drawRoundedRectangle (rc (r).reduced (0.5f), 5.0f, 1.0f);
    }

    void drawVuFrame (Graphics& g)
    {
        const auto r = rc (layout::vuwrap);
        Path body;
        body.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 14.0f, 14.0f, true, true, false, false);
        Path lower;
        lower.addRoundedRectangle (r.getX(), r.getY() + 20.0f, r.getWidth(), r.getHeight() - 20.0f, 10.0f, 10.0f, false, false, true, true);
        body.addPath (lower);
        body.setUsingNonZeroWinding (true);
        DropShadow (Colours::black.withAlpha (0.55f), 14, { 0, 6 }).drawForPath (g, body);
        ColourGradient gr (Colour (0xff0b0b0d), 0.0f, r.getY(), Colour (0xff17181b), 0.0f, r.getBottom(), false);
        g.setGradientFill (gr); g.fillPath (body);
        g.setColour (Colour (0xff2d3036)); g.strokePath (body, PathStrokeType (2.0f));
        g.setColour (Colours::black); g.drawRoundedRectangle (r.reduced (2.0f), 10.0f, 2.0f);
        // the cat ears on top of the meter (svg units 460 x 26 at layout::ears)
        const auto e = rc (layout::ears);
        auto P = [&] (float x, float y) { return Point<float> (e.getX() + x, e.getY() + y); };
        auto tri = [&] (Point<float> a, Point<float> b, Point<float> c2) { Path p; p.addTriangle (a, b, c2); return p; };
        for (int side = 0; side < 2; ++side)
        {
            Path outer = side == 0 ? tri (P (30, 26), P (48, 2), P (84, 26)) : tri (P (430, 26), P (412, 2), P (376, 26));
            Path inner = side == 0 ? tri (P (44, 24), P (52, 11), P (70, 24)) : tri (P (416, 24), P (408, 11), P (390, 24));
            g.setColour (Colour (0xff17181b)); g.fillPath (outer);
            g.setColour (Colour (0xff2d3036)); g.strokePath (outer, PathStrokeType (2.0f));
            g.setColour (theme::pink.withAlpha (0.75f)); g.fillPath (inner);
        }
    }

    void timerCallback() override
    {
        const bool showing = forceLive || isShowing();
        if (! showing)
        {
            // hidden window (another tab, minimised, closed): do no work, wake up slowly to notice it is back
            if (! gated) { gated = true; startTimer (250); }
            return;
        }
        if (gated) { gated = false; startTimerHz (ctx.power ? 16 : 60); }
        const float dt = 1.0f / (float) (ctx.power ? 16 : 60);
        refreshLive (dt);
        stepNeedle (dt);
        vu.repaint(); hist.repaint(); curve.repaint(); filt.repaint();
        meterIn.update (live.inPk); meterOut.update (live.outPk);
        repaint (Rectangle<int> (roundToInt (layout::pkIn.x) - 10, roundToInt (layout::pkIn.y), 80, roundToInt (layout::pkIn.h)));
        syncControls();
    }

    float real (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setReal (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
    void toggle (const char* id) { setReal (id, real (id) > 0.5f ? 0.0f : 1.0f); syncControls(); }

    void refreshLive (float dt)
    {
        live.fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
        const float dec = std::exp (-dt / 0.1f);
        const uint32_t nb = proc.blocks.load (std::memory_order_relaxed);
        const bool flowing = nb != lastBlocks;
        lastBlocks = nb;
        live.inPk = std::max (proc.inPeak.exchange (0.0f), live.inPk * dec);
        live.outPk = std::max (proc.outPeak.exchange (0.0f), live.outPk * dec);
        const float peakGr = proc.grPeakDb.exchange (0.0f);
        if (flowing)
        {
            live.grNow = proc.grNowDb.load(); grAvg = proc.grAvgDb.load(); vuE = proc.vuEnergy.load(); live.detIn = proc.detInDb.load();
        }
        else
        {
            // no audio is running: let the displays settle instead of freezing on the last block
            live.grNow *= 0.9f; grAvg *= 0.9f; vuE *= 0.9f; live.detIn = std::max (-90.0f, live.detIn - 3.0f);
        }
        live.hist[live.hi] = flowing ? peakGr : live.grNow;
        live.hi = (live.hi + 1) % 360;
        live.peak = real (ids::peak); live.emph = real (ids::emph);
        live.mode = (int) real (ids::mode); live.meter = (int) real (ids::meter);
        live.bypass = real (ids::bypass) > 0.5f;
    }

    // second order needle, damping 0.8: about 1.5 percent overshoot, 99 percent in about 0.3 s (the mockup's setting)
    void stepNeedle (float dt)
    {
        const float tgt = live.meter == 0 ? grAvg : 10.0f * std::log10 (std::max (vuE, 1.0e-12f)) - (live.meter == 1 ? -8.0f : -14.0f);
        const float tv = live.meter == 0 ? jlimit (0.0f, 40.0f, tgt) : jlimit (-22.0f, 4.0f, tgt);
        const float NW = 13.2f, NZ = 0.8f;
        const int k = std::max (1, (int) std::ceil (dt / 0.004f));
        const float h = dt / (float) k;
        for (int i = 0; i < k; ++i)
        {
            live.nvel += (NW * NW * (tv - live.needle) - 2.0f * NZ * NW * live.nvel) * h;
            live.needle += live.nvel * h;
        }
    }

    String dot() const { return String::fromUTF8 ("\xc2\xb7"); }
    void syncName() { pName->setText (presetName() + (dirty() ? " *" : "")); }

    void syncControls()
    {
        bBypass->setOn (live.bypass);
        const float pk = real (ids::peak), ga = real (ids::gain), em = real (ids::emph), mx = real (ids::mix);
        values[0]->setParts (String (roundToInt (ga)), dot() + " +" + String (ga * 0.4f, 1) + " dB");
        values[1]->setParts (String (roundToInt (pk)), dot() + " thr " + String (-pk * 0.4f, 1) + " dB");
        values[2]->setParts (String (roundToInt (mx)), "%");
        if (em < 0.5f) values[3]->setParts ("0", dot() + " off");
        else values[3]->setParts (String (roundToInt (em)), dot() + " lows -" + String (-20.0f * std::log10 (1.0f - 0.85f * em / 100.0f), 1) + " dB");
        for (int i = 0; i < 3; ++i) segMeter[i]->setOn (live.meter == i);
        for (int i = 0; i < 2; ++i) segMode[i]->setOn (live.mode == i);
        pName->setText (presetName() + (dirty() ? " *" : ""));
    }

    // ---- building ----------------------------------------------------------------------------
    PButton* addButton (const String& text, const layout::Rc& r, const String& tip, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
    {
        auto* b = new PButton (ctx, text, px, k);
        owned.add (b);
        addAndMakeVisible (b);
        place (*b, r);
        b->tip = tip;
        return b;
    }

    void buildHeader()
    {
        pPrev = addButton (String::fromUTF8 ("\xe2\x97\x80"), layout::pPrev, layout::tip_pPrev);
        pName = addButton (presetName(), layout::pName, layout::tip_pName, 16.0f, theme::FontKind::semi);
        pNext = addButton (String::fromUTF8 ("\xe2\x96\xb6"), layout::pNext, layout::tip_pNext);
        pSave = addButton ("Save", layout::pSave, layout::tip_pSave);
        bBypass = addButton ("Bypass", layout::bBypass, layout::tip_bBypass);
        bGuide = addButton ("Guide", layout::bGuide, layout::tip_bGuide);
        bSettings = addButton ("Settings", layout::bSettings, layout::tip_bSettings);
        bAbout = addButton ("About & Support", layout::bAbout, layout::tip_bAbout);
        pPrev->onClick = [this] (const ModifierKeys&) { step (-1); };
        pNext->onClick = [this] (const ModifierKeys&) { step (1); };
        pSave->onClick = [this] (const ModifierKeys&) { modal.openSave (nullptr); };
        bBypass->onClick = [this] (const ModifierKeys&) { toggle (ids::bypass); };
        bGuide->onClick = [this] (const ModifierKeys&) { modal.openGuide (0); };
        bSettings->onClick = [this] (const ModifierKeys&) { modal.openSettings(); };
        bAbout->onClick = [this] (const ModifierKeys&) { modal.openAbout(); };
        pName->onClick = [this] (const ModifierKeys&) { nameDelay.start (260); };
        nameDelay.fn = [this] { modal.openBrowser(); };
        pName->onDouble = [this]
        {
            nameDelay.stopTimer();
            const Preset* p = presetStore.find (presetName());
            if (p == nullptr) p = &presets::factory()[0];
            Preset q = *p; modal.openSave (&q);
        };
    }

    void step (int d)
    {
        auto all = presetStore.all();
        int i = 0;
        for (size_t k = 0; k < all.size(); ++k) if (all[k].name == presetName()) i = (int) k;
        i = (i + d + (int) all.size()) % (int) all.size();
        loadPreset (all[(size_t) i]);
    }

    void buildHardware()
    {
        const char* pid[4] = { ids::gain, ids::peak, ids::mix, ids::emph };
        for (int i = 0; i < 4; ++i)
        {
            auto& kc = layout::knobs[i];
            auto* k = new KnobView (*proc.apvts.getParameter (pid[i]), kc.big);
            knobs.add (k); addAndMakeVisible (k);
            place (*k, kc.knob);
            k->tip = kc.tip;
            auto* v = new KnobValue (ctx, kc.big);
            values.add (v); addAndMakeVisible (v);
            place (*v, kc.val);
            if (! kc.big) v->setBounds (roundToInt (kc.val.x), roundToInt (kc.val.y), roundToInt (kc.val.w) + 40, roundToInt (kc.val.h));
            else v->setBounds (roundToInt (kc.val.x + kc.val.w * 0.5f - 90.0f), roundToInt (kc.val.y), 180, roundToInt (kc.val.h));
        }
        // VU meter
        addAndMakeVisible (vu);
        place (vu, layout::cvVU);
        vu.tip = layout::tip_cvVU;
        // switches
        for (int i = 0; i < 3; ++i)
        {
            auto& b = layout::segMeterBtns[i];
            auto* s = new SegButton (ctx, b.text, 14.0f, i == 2);
            segMeter.add (s); addAndMakeVisible (s);
            place (*s, b.r);
            s->tip = b.tip;
            s->onClick = [this, i] { setReal (ids::meter, (float) i); syncControls(); };
        }
        for (int i = 0; i < 2; ++i)
        {
            auto& b = layout::segModeBtns[i];
            auto* s = new SegButton (ctx, b.text, 16.0f, i == 1);
            segMode.add (s); addAndMakeVisible (s);
            place (*s, b.r);
            s->tip = b.tip;
            s->onClick = [this, i] { setReal (ids::mode, (float) i); syncControls(); };
        }
    }

    void buildDisplays()
    {
        CanvasView* views[3] = { &curve, &hist, &filt };
        const layout::Rc* rects[3] = { &layout::cvCurve, &layout::cvHist, &layout::cvFilt };
        const char* tips[3] = { layout::tip_cvCurve, layout::tip_cvHist, layout::tip_cvFilt };
        for (int i = 0; i < 3; ++i)
        {
            addAndMakeVisible (*views[i]);
            place (*views[i], *rects[i]);
            views[i]->tip = tips[i];
        }
        auto gestureSet = [this] (const char* id, float v, bool first)
        {
            auto* p = proc.apvts.getParameter (id);
            if (first) p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            dragParam = p;
        };
        auto endGesture = [this] { if (dragParam != nullptr) { dragParam->endChangeGesture(); dragParam = nullptr; } };
        curve.onDragAt = [gestureSet] (float cx, float, bool first)
        {
            const float db = ((cx - 24.0f) / (282.0f - 24.0f - 6.0f)) * 60.0f - 60.0f;
            gestureSet (ids::peak, (float) jlimit (0.0, 100.0, std::round (-db / 0.4)), first);
        };
        curve.onDragEnd = endGesture;
        filt.onDragAt = [gestureSet] (float, float cy, bool first)
        {
            const float lowsDb = jlimit (0.0f, 16.5f, -((1.0f - (cy - 6.0f) / (112.0f - 12.0f - 6.0f)) * 21.0f - 18.0f));
            gestureSet (ids::emph, (float) jlimit (0.0, 100.0, std::round ((1.0 - d2l (-lowsDb)) / 0.85 * 100.0)), first);
        };
        filt.onDragEnd = endGesture;
    }

    void buildMeters()
    {
        addAndMakeVisible (meterIn); addAndMakeVisible (meterOut);
        place (meterIn, layout::cvIn); place (meterOut, layout::cvOut);
        meterIn.tip = layout::tip_cvIn; meterOut.tip = layout::tip_cvOut;
        auto* a = new TipArea(); owned.add (a); addAndMakeVisible (a);
        a->setBounds (roundToInt (layout::pkIn.x) - 10, roundToInt (layout::pkIn.y), 36, roundToInt (layout::pkIn.h));
        a->tip = layout::tip_pkIn;
        auto* b = new TipArea(); owned.add (b); addAndMakeVisible (b);
        b->setBounds (roundToInt (layout::pkOut.x) + 2, roundToInt (layout::pkOut.y), 36, roundToInt (layout::pkOut.h));
        b->tip = layout::tip_pkOut;
    }

    // ---- tips --------------------------------------------------------------------------------
    void trackTip (const MouseEvent& e)
    {
        if (! ctx.tips) { hideTip(); return; }
        Component* c = e.eventComponent;
        String text; Component* host = nullptr;
        while (c != nullptr && c != this)
        {
            if (auto* tc = dynamic_cast<TipClient*> (c); tc != nullptr && tc->tip.isNotEmpty())
            {
                text = tc->tip; host = c; break;
            }
            c = c->getParentComponent();
        }
        if (host == nullptr || text.isEmpty()) { hideTip(); return; }
        if (host == tipHost && text == tipText) return;
        hideTip();
        tipHost = host; tipText = text;
        tipTimer.start (1000);
    }
    void showTip()
    {
        if (tipHost == nullptr) return;
        bubble.showAt (tipText, getLocalArea (tipHost, tipHost->getLocalBounds()), getWidth(), getHeight());
    }
    void hideTip() { tipTimer.stopTimer(); bubble.setVisible (false); tipHost = nullptr; tipText = {}; }

    // ---- members -----------------------------------------------------------------------------
    LapCat2AProcessor& proc;
    std::function<void()> onResetSize;
    Ctx ctx;
    LapLnF lnf;
    Live live;
    float grAvg = 0.0f, vuE = 0.0f;
    uint32_t lastBlocks = 0;
    bool gated = false, forceLive = false;
    RangedAudioParameter* dragParam = nullptr;
    presets::Store presetStore;
    std::unique_ptr<Drawable> logo;
    Cheek* cheekL = nullptr; Cheek* cheekR = nullptr;
    OwnedArray<Component> owned;
    PButton *pPrev = nullptr, *pName = nullptr, *pNext = nullptr, *pSave = nullptr, *bBypass = nullptr, *bGuide = nullptr, *bSettings = nullptr, *bAbout = nullptr;
    VUView vu;
    CurveView curve; FiltView filt; HistView hist;
    OwnedArray<KnobView> knobs;
    OwnedArray<KnobValue> values;
    OwnedArray<SegButton> segMeter, segMode;
    MeterView meterIn, meterOut;
    Bubble bubble; Toast toastView;
    Modal modal;
    Delay nameDelay, tipTimer;
    Component* tipHost = nullptr;
    String tipText;
};

// =====================================================================================================
LapCat2AEditor::LapCat2AEditor (LapCat2AProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    face = std::make_unique<Face> (p, [this] { resetSize(); });
    addAndMakeVisible (*face);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    constrainer.setMinimumSize (440, 238);
    constrainer.setMaximumSize (1650, 894);
    setConstrainer (&constrainer);
    setResizable (true, false);
    border = std::make_unique<ResizableBorderComponent> (this, &constrainer);
    border->setBorderThickness (BorderSize<int> (5));
    addAndMakeVisible (*border);
    const double sc = jlimit (0.4, 1.5, (double) proc.getSetting (setting::scale, 1.0));
    setSize (roundToInt (kW * sc), roundToInt (kH * sc));
}

LapCat2AEditor::~LapCat2AEditor() = default;

void LapCat2AEditor::resized()
{
    const double s = (double) getWidth() / (double) kW;
    face->setTransform (AffineTransform::scale ((float) s));
    face->setBounds (0, 0, kW, kH);
    border->setBounds (getLocalBounds());
    proc.setSetting (setting::scale, s);
}

void LapCat2AEditor::resetSize() { setSize (kW, kH); }
void LapCat2AEditor::openDialogForTesting (const juce::String& n) { face->openForTesting (n); }
