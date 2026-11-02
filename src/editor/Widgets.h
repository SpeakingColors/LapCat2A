#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "Theme.h"
#include "Layout.h"

namespace ui
{
using namespace juce;

// Settings every widget needs to paint (look and info bubbles).
struct Ctx
{
    char look = 'C';
    bool tips = true;
    bool power = false;
    const theme::Look& t() const { return theme::look (look); }
};

// Anything that can show an info bubble.
struct TipClient
{
    String tip;
    virtual ~TipClient() = default;
};

// A text box that can also show an info bubble.
class TipEditor : public TextEditor, public TipClient {};

inline Rectangle<float> rc (const layout::Rc& r) { return { r.x, r.y, r.w, r.h }; }

inline float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// Text centred in a box, never shortened with dots. Letter spacing is in px.
inline void drawText (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c,
                      Justification j = Justification::centred, float spacingPx = 0.0f)
{
    if (std::abs (spacingPx) > 1.0e-6f) f = f.withExtraKerningFactor (spacingPx / jmax (1.0f, f.getHeight())); // JUCE kerning is a fraction of the font height
    g.setFont (f);
    g.setColour (c);
    Rectangle<float> box = r.expanded (30.0f, 0.0f); // room for font differences, text is never shortened
    if (j.testFlags (Justification::left))       box = r.withWidth (r.getWidth() + 60.0f);
    else if (j.testFlags (Justification::right)) box = r.withLeft (r.getX() - 60.0f);
    g.drawText (s, box, j, false);
}

// ---- Button in the mockup's .btn style -----------------------------------------------------------
class PButton : public Component, public TipClient
{
public:
    PButton (const Ctx& c, String t, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
        : text (std::move (t)), ctx (c), size (px), kind (k) { setMouseCursor (MouseCursor::PointingHandCursor); }

    std::function<void (const ModifierKeys&)> onClick;
    std::function<void()> onDouble;
    bool on = false;
    bool hot = false;                       // red border and pink text (Auto Trim has trimmed)
    float spacing = 0.5f;
    float radius = 4.0f;
    String text;

    void setText (const String& t) { if (t != text) { text = t; repaint(); } }
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }

    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (on ? theme::sel : t.plate);
        g.fillRoundedRectangle (r, radius);
        Colour border = hot ? theme::red : on ? theme::selb : (over ? t.dim : t.line);
        g.setColour (border);
        g.drawRoundedRectangle (r, radius, 1.0f);
        const Colour tc = hot ? theme::pink : on ? theme::lilac : t.ink;
        auto f = theme::font (kind, size);
        if (text.contains ("\n"))
        {
            auto lines = StringArray::fromLines (text);
            const float lh = size * 1.15f;
            float y = r.getCentreY() - lh * (float) lines.size() * 0.5f;
            for (auto& l : lines) { drawText (g, l, { r.getX(), y, r.getWidth(), lh }, f, tc, Justification::centred, spacing); y += lh; }
        }
        else drawText (g, text, r, f, tc, Justification::centred, spacing);
    }
    void mouseEnter (const MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick (e.mods);
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onDouble) onDouble(); }

private:
    const Ctx& ctx;
    float size;
    theme::FontKind kind;
    bool over = false;
};

// ---- Segmented switch button (the container is drawn by the faceplate) ------------------------------
class SegButton : public Component, public TipClient
{
public:
    SegButton (const Ctx& c, String t, float px, bool last) : ctx (c), text (std::move (t)), size (px), isLast (last) { setMouseCursor (MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    bool on = false;
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat();
        if (on)
        {
            g.setColour (theme::sel); g.fillRect (r);
            g.setColour (theme::selb); g.fillRect (r.removeFromBottom (2.0f));
            r = getLocalBounds().toFloat();
        }
        if (! isLast) { g.setColour (t.line); g.fillRect (r.getRight() - 1.0f, 0.0f, 1.0f, r.getHeight()); }
        drawText (g, text, r, theme::font (theme::FontKind::regular, size), on ? theme::lilac : (over ? t.ink : t.dim), Justification::centred, 1.0f);
    }
    void mouseEnter (const MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const MouseEvent& e) override { if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
private:
    const Ctx& ctx;
    String text;
    float size;
    bool isLast, over = false;
};

// ---- Faceplate knob, drawn like the mockup's SVG (150 x 150 units; the small knob is scaled) ----------
class KnobView : public Component, public TipClient
{
public:
    KnobView (RangedAudioParameter& prm, bool isBig)
        : p (prm), big (isBig), att (prm, [this] (float) { repaint(); })
    {
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (true);
        att.sendInitialUpdate();
    }

    double norm() const { return p.getValue(); }

    void paint (Graphics& g) override
    {
        g.addTransform (AffineTransform::scale ((float) getWidth() / 150.0f));
        const float cx = 75.0f, cy = 75.0f;
        for (int i = 0; i <= 20; ++i)
        {
            const float a = degreesToRadians (-135.0f + (float) i * 13.5f);
            const bool major = i % 2 == 0;
            const float r1 = major ? 65.0f : 68.0f, r2 = 73.0f;
            g.setColour (major ? Colour (0xfff4f5f7) : Colour (0xff8c8f99));
            g.drawLine (cx + std::sin (a) * r1, cy - std::cos (a) * r1, cx + std::sin (a) * r2, cy - std::cos (a) * r2, major ? 2.2f : 1.2f);
        }
        if (big)
        {
            g.setFont (theme::font (theme::FontKind::semi, 10.0f));
            g.setColour (Colour (0xffc9cbd3));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = degreesToRadians (-135.0f + (float) i * 27.0f);
                g.drawSingleLineText (String (i * 10), roundToInt (cx + std::sin (a) * 55.0f), roundToInt (cy - std::cos (a) * 55.0f + 3.5f),
                                      Justification::horizontallyCentred);
            }
        }
        const float rDark = big ? 44.0f : 56.0f, rCap = big ? 36.0f : 50.0f, rRing = big ? 32.0f : 45.0f;
        g.setColour (Colour (0xff0b0b0d)); g.fillEllipse (cx - rDark, cy - rDark, rDark * 2, rDark * 2);
        g.setColour (Colours::black);      g.drawEllipse (cx - rDark, cy - rDark, rDark * 2, rDark * 2, 1.0f);
        {
            const float d = rCap * 2.0f, gx = cx - rCap + 0.4f * d, gy = cy - rCap + 0.35f * d;
            ColourGradient grad (Colour (0xfff1f1f4), gx, gy, Colour (0xff6e6f78), gx + 0.7f * d, gy, true);
            grad.addColour (0.55, Colour (0xffb9bac2));
            g.setGradientFill (grad);
            g.fillEllipse (cx - rCap, cy - rCap, d, d);
        }
        g.setColour (Colour (0xff4a4b53)); g.drawEllipse (cx - rCap, cy - rCap, rCap * 2, rCap * 2, 1.0f);
        g.setColour (Colours::black.withAlpha (0.25f)); g.drawEllipse (cx - rRing, cy - rRing, rRing * 2, rRing * 2, 1.0f);
        Path ptr;
        ptr.startNewSubPath (cx, cy); ptr.lineTo (cx, big ? 46.0f : 32.0f);
        Path stroked;
        PathStrokeType (big ? 4.0f : 7.0f, PathStrokeType::curved, PathStrokeType::rounded).createStrokedPath (stroked, ptr);
        stroked.applyTransform (AffineTransform::rotation (degreesToRadians (-135.0f + 270.0f * (float) norm()), cx, cy));
        g.setColour (Colour (0xff16171a));
        g.fillPath (stroked);
        if (hasKeyboardFocus (false))
        {
            g.setColour (theme::selb);
            g.drawEllipse (1.0f, 1.0f, 148.0f, 148.0f, 3.0f);
        }
    }

    void mouseDown (const MouseEvent&) override { startNorm = norm(); att.beginGesture(); grabKeyboardFocus(); }
    void mouseDrag (const MouseEvent& e) override
    {
        // the knob sits under the editor's scale transform, so the drag distance is in window pixels
        const double d = -(double) e.getDistanceFromDragStartY() / 250.0 * (e.mods.isShiftDown() ? 0.1 : 1.0);
        setNorm (startNorm + d);
    }
    void mouseUp (const MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const MouseEvent&) override { att.beginGesture(); setNorm (p.getDefaultValue()); att.endGesture(); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const float dy = w.isReversed ? -w.deltaY : w.deltaY;
        if (std::abs (dy) < 1.0e-6f) return;
        att.beginGesture();
        setNorm (norm() + (dy > 0 ? 1 : -1) * (e.mods.isShiftDown() ? 0.002 : 0.02));
        att.endGesture();
    }
    bool keyPressed (const KeyPress& k) override
    {
        const double s = k.getModifiers().isShiftDown() ? 0.002 : 0.01;
        double n = norm();
        if (k.isKeyCode (KeyPress::upKey) || k.isKeyCode (KeyPress::rightKey)) n += s;
        else if (k.isKeyCode (KeyPress::downKey) || k.isKeyCode (KeyPress::leftKey)) n -= s;
        else if (k.isKeyCode (KeyPress::homeKey)) n = 0.0;
        else if (k.isKeyCode (KeyPress::endKey)) n = 1.0;
        else return false;
        att.beginGesture(); setNorm (n); att.endGesture();
        return true;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    void setNorm (double n)
    {
        n = jlimit (0.0, 1.0, n);
        att.setValueAsPartOfGesture (p.convertFrom0to1 ((float) n));
    }

private:
    RangedAudioParameter& p;
    bool big;
    ParameterAttachment att;
    double startNorm = 0.0;
};

// Value text under or beside a knob: "20 . +8.0 dB". The first part is bright, the rest dim.
class KnobValue : public Component
{
public:
    explicit KnobValue (const Ctx& c, bool centre) : ctx (c), centred (centre) { setInterceptsMouseClicks (false, false); }
    String bold, rest;
    void setParts (const String& b, const String& r) { if (b != bold || r != rest) { bold = b; rest = r; repaint(); } }

    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto f = theme::font (theme::FontKind::mono, 12.0f);
        auto r = getLocalBounds().toFloat();
        const float wb = textWidth (f, bold), wr = textWidth (f, " " + rest);
        float x = centred ? r.getCentreX() - (wb + wr) * 0.5f : r.getX();
        const float y = r.getCentreY() + f.getAscent() * 0.5f - 1.0f;
        g.setFont (f);
        g.setColour (t.ink);
        g.drawSingleLineText (bold, roundToInt (x), roundToInt (y));
        g.setColour (t.dim);
        g.drawSingleLineText (" " + rest, roundToInt (x + wb), roundToInt (y));
    }
private:
    const Ctx& ctx;
    bool centred;
};
} // namespace ui

namespace ui
{
// Colours and fonts for the pieces JUCE draws itself (the look drop-down and its pop-up list).
class LapLnF : public LookAndFeel_V4
{
public:
    explicit LapLnF (const Ctx& c) : ctx (c) { refresh(); }
    void refresh()
    {
        auto& t = ctx.t();
        setColour (PopupMenu::backgroundColourId, t.disp);
        setColour (PopupMenu::textColourId, t.ink);
        setColour (PopupMenu::highlightedBackgroundColourId, theme::sel);
        setColour (PopupMenu::highlightedTextColourId, theme::lilac);
        setColour (ComboBox::backgroundColourId, t.disp);
        setColour (ComboBox::textColourId, t.ink);
        setColour (ComboBox::outlineColourId, t.line);
        setColour (ComboBox::arrowColourId, t.dim);
        setColour (ComboBox::focusedOutlineColourId, theme::selb);
    }
    Font getPopupMenuFont() override { return theme::font (theme::FontKind::regular, 15.0f); }
    Font getComboBoxFont (ComboBox&) override { return theme::font (theme::FontKind::regular, 15.0f); }
private:
    const Ctx& ctx;
};
} // namespace ui
