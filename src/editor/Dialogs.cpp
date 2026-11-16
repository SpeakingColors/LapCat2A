#include "Dialogs.h"
#include "Studio87Link.h"
#include "../editor/Layout.h"
#include <map>

namespace ui
{
namespace
{
// ---- little building blocks -----------------------------------------------------------------------
struct Seg { String text; bool bold = false, mono = false, dim = false, accent = false; int tone = 0; }; // tone: 1 green, 2 yellow, 3 red

class TextBlock : public Component
{
public:
    TextBlock (const Ctx& c, std::vector<Seg> s, float px = 15.0f) : ctx (c), segs (std::move (s)), size (px) { setInterceptsMouseClicks (false, false); }
    void setSize (float px) { size = px; }
    int heightFor (int w) const
    {
        TextLayout tl; tl.createLayout (build(), (float) w);
        return (int) std::ceil (tl.getHeight()) + 2;
    }
    void setSegs (std::vector<Seg> s) { segs = std::move (s); repaint(); }
    void paint (Graphics& g) override { build().draw (g, getLocalBounds().toFloat()); }
private:
    AttributedString build() const
    {
        auto& t = ctx.t();
        AttributedString as;
        as.setWordWrap (AttributedString::byWord);
        as.setLineSpacing (3.0f);
        for (auto& s : segs)
            as.append (s.text, theme::font (s.mono ? theme::FontKind::mono : s.bold ? theme::FontKind::bold : theme::FontKind::regular, s.mono ? size - 3.0f : size),
                       s.tone == 1 ? theme::green : s.tone == 2 ? t.yellow : s.tone == 3 ? theme::red : s.accent ? theme::selb : s.dim ? t.dim : t.ink);
        return as;
    }
    const Ctx& ctx;
    std::vector<Seg> segs;
    float size;
};

class Kbd : public Component
{
public:
    Kbd (const Ctx& c, String s) : ctx (c), text (std::move (s)) { setInterceptsMouseClicks (false, false); }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto f = theme::font (theme::FontKind::mono, 12.0f);
        const float w = textWidth (f, text) + 12.0f;
        Rectangle<float> r (0.0f, 2.0f, w, 20.0f);
        g.setColour (t.disp); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (t.line); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        drawText (g, text, r, f, t.ink);
    }
private:
    const Ctx& ctx;
    String text;
};

class PToggle : public Component, public TipClient
{
public:
    PToggle (const Ctx& c, String s, bool v) : ctx (c), text (std::move (s)), value (v) { setMouseCursor (MouseCursor::PointingHandCursor); }
    std::function<void (bool)> onChange;
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        Rectangle<float> box (0.0f, (getHeight() - 16) * 0.5f, 16.0f, 16.0f);
        g.setColour (t.disp); g.fillRoundedRectangle (box, 3.0f);
        g.setColour (value ? theme::selb : t.line); g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
        if (value)
        {
            Path p; p.startNewSubPath (box.getX() + 3.5f, box.getCentreY()); p.lineTo (box.getX() + 7.0f, box.getBottom() - 4.5f); p.lineTo (box.getRight() - 3.0f, box.getY() + 4.0f);
            g.setColour (t.accent); g.strokePath (p, PathStrokeType (2.0f));
        }
        drawText (g, text, getLocalBounds().toFloat().withTrimmedLeft (24.0f), theme::font (theme::FontKind::regular, 15.0f), t.ink, Justification::centredLeft);
    }
    void mouseUp (const MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        value = ! value; repaint();
        if (onChange) onChange (value);
    }
private:
    const Ctx& ctx;
    String text;
    bool value;
};

// Info bubbles for every control in a dialog, as in the mockup.
String dialogTip (const String& name, const String& text, bool chip)
{
    static const std::map<String, String> m {
        { "pbQ", "Filters the list as you type, by name, type or tag. Try typing thicken to find the supersaw presets." },
        { "svN", "The name for your preset. Try a name that says what it is for." },
        { "svG", "Tags help you find the preset later. Try a few short words separated by commas." },
        { "exT", "The exported preset text. Click it to select everything, then copy it and keep it." },
        { "imT", "Paste exported preset text here. Anything invalid is refused." },
        { "Load", "Loads the selected preset into the plugin. Try double-clicking a name in the list instead." },
        { "Copy and rename", "Makes your own copy of the selected factory preset so you can change it." },
        { "Rename", "Renames your preset." },
        { "Delete", "Deletes your preset. It cannot be undone, so export it first if you want to keep it." },
        { "Export", "Shows the selected preset as text you can copy and keep." },
        { "Import", "Lets you paste preset text exported earlier. Try it to move presets between computers." },
        { "Export all", "Shows all your own presets as text to copy and keep." },
        { "Done", "Closes this window." },
        { "Cancel", "Closes this window without changing anything." },
        { "Save", "Saves the preset under this name." },
        { "Save as new", "Keeps the old preset and saves this one next to it under a new number." },
        { "Overwrite", "Replaces the existing preset with the same name." },
        { "Copy", "Copies the text to the clipboard." },
        { "Close", "Closes this window. The Escape key does the same." },
        { "Reset window to 100%", "Puts the window back to its normal size." },
        { "Basics", "Shows the controls, shortcuts and where to start." },
        { "Thicken a supersaw", "Shows how to use the LA-2A style stage to thicken supersaw leads and pads, with four presets you can load." },
        { "Model: verified and tuned", "Shows what is verified from published sources, what is our own tuning and what is missing." },
        { "GitHub", "Opens this link in your browser. The plugin itself never goes online, and nothing is sent." },
        { "Releases", "Opens this link in your browser. The plugin itself never goes online, and nothing is sent." },
        { "Ko-fi studio87", "Opens this link in your browser. The plugin itself never goes online, and nothing is sent." },
        { "<", "Shows the previous page of presets. Try it when the list is longer than one page." },
        { ">", "Shows the next page of presets. Try it when the list is longer than one page." } };
    auto it = m.find (name.isNotEmpty() ? name : text);
    if (it != m.end()) return it->second;
    it = m.find (text);
    if (it != m.end()) return it->second;
    if (chip) return text == "All" ? "Shows every preset." : "Shows only " + text + " presets. Try it, then pick one in the list.";
    return "Use this control. Try it and see what changes.";
}

void autoTip (Component& root)
{
    for (auto* c : root.getChildren())
    {
        if (auto* b = dynamic_cast<PButton*> (c)) { if (b->tip.isEmpty()) b->tip = dialogTip (b->getName(), b->text, b->radius > 8.0f); }
        else if (auto* e = dynamic_cast<TipEditor*> (c)) { if (e->tip.isEmpty()) e->tip = dialogTip (e->getName(), {}, false); }
        else if (auto* s = dynamic_cast<SegButton*> (c)) { (void) s; }
        autoTip (*c);
    }
}

void styleEditor (TextEditor& e, const Ctx& c, bool multi)
{
    auto& t = c.t();
    e.setColour (TextEditor::backgroundColourId, t.disp);
    e.setColour (TextEditor::textColourId, t.ink);
    e.setColour (TextEditor::outlineColourId, t.line);
    e.setColour (TextEditor::focusedOutlineColourId, theme::selb);
    e.setColour (TextEditor::highlightColourId, theme::sel);
    e.setColour (TextEditor::highlightedTextColourId, theme::lilac);
    e.setColour (CaretComponent::caretColourId, t.ink);
    e.setFont (theme::font (multi ? theme::FontKind::mono : theme::FontKind::regular, multi ? 12.0f : 15.0f));
    e.setIndents (8, 5);
    e.setBorder ({ 1, 1, 1, 1 });
    e.setMultiLine (multi, multi);
    e.setReturnKeyStartsNewLine (multi);
}
} // namespace

// ---- Panel: title bar, rows, footer buttons -------------------------------------------------------
class Panel : public Component
{
public:
    Panel (Modal& m, DialogHost& h, String t, int w) : modal (m), host (h), title (std::move (t)), width (w), closeBtn (h.context(), "Close")
    {
        closeBtn.onClick = [this] (const ModifierKeys&) { modal.later ([this] { modal.close(); }); };
        closeBtn.tip = dialogTip ("Close", {}, false);
        addAndMakeVisible (closeBtn);
        setSize (w, 200);
    }
    struct Row
    {
        std::vector<Component*> items;
        std::vector<int> widths;                 // 0 = share of the remaining width
        int height = 24;
        std::function<int (int)> heightFor;
        int gap = 6;
    };
    template <class T, class... A> T* make (A&&... a)
    {
        auto p = std::make_unique<T> (std::forward<A> (a)...);
        T* raw = p.get();
        addAndMakeVisible (*raw);
        owned.push_back (std::move (p));
        return raw;
    }
    void row (std::vector<Component*> items, std::vector<int> widths, int height, int gap = 6)
    {
        Row r; r.items = std::move (items); r.widths = std::move (widths); r.height = height; r.gap = gap;
        rows.push_back (std::move (r));
    }
    void textRow (std::vector<Seg> segs, int gap = 8)
    {
        auto* tb = make<TextBlock> (host.context(), std::move (segs));
        Row r; r.items = { tb }; r.widths = { 0 }; r.gap = gap;
        r.heightFor = [tb] (int w) { return tb->heightFor (w); };
        rows.push_back (std::move (r));
    }
    void kvRow (Component* key, TextBlock* desc, int keyW)
    {
        Row r; r.items = { key, desc }; r.widths = { keyW, 0 }; r.gap = 2;
        r.heightFor = [desc, keyW, this] (int w) { (void) this; return std::max (24, desc->heightFor (w - keyW - 8)); };
        rows.push_back (std::move (r));
    }
    PButton* footer (const String& text, std::function<void()> fn, bool primary = false)
    {
        auto* b = make<PButton> (host.context(), text);
        b->on = primary;
        b->onClick = [this, fn] (const ModifierKeys&) { modal.later (fn); };
        footers.push_back (b);
        return b;
    }
    void fit (int maxH = layout::H - 24)
    {
        const int h = std::min (maxH, layoutRows (false) + 12 + (footers.empty() ? 0 : kFooter));
        setSize (width, h);
        resized();
        autoTip (*this);
    }
    void paint (Graphics& g) override
    {
        auto& t = host.context().t();
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (0, 6).expanded (6), 12.0f);
        g.setColour (t.plate); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (t.line);
        g.fillRect (1, kTitle, getWidth() - 2, 1);
        if (! footers.empty()) g.fillRect (1, getHeight() - kFooter, getWidth() - 2, 1);
        drawText (g, title, { 16.0f, 0.0f, (float) getWidth() - 100.0f, (float) kTitle }, theme::font (theme::FontKind::regular, 20.0f), t.title, Justification::centredLeft, 1.0f);
    }
    void resized() override
    {
        closeBtn.setBounds (getWidth() - 12 - 60, (kTitle - 26) / 2, 60, 26);
        layoutRows (true);
        int x = getWidth() - 16;
        for (int i = (int) footers.size(); --i >= 0;)
        {
            const int w = std::max(70, (int) textWidth (theme::font (theme::FontKind::regular, 14.0f), footers[(size_t) i]->text) + 36);
            footers[(size_t) i]->setBounds (x - w, getHeight() - kFooter + 11, w, 28);
            x -= w + 8;
        }
    }
    Modal& modal;
    DialogHost& host;
    String title;
    int width;
    PButton closeBtn;
    static constexpr int kTitle = 44, kFooter = 50;
private:
    int layoutRows (bool place)
    {
        const int bw = width - 32;
        int y = kTitle + 12;
        for (auto& r : rows)
        {
            const int h = r.heightFor ? r.heightFor (bw) : r.height;
            if (place)
            {
                int fixed = 0, flex = 0;
                for (int w : r.widths) { if (w > 0) fixed += w; else ++flex; }
                const int gaps = (int) r.items.size() - 1;
                const int share = flex > 0 ? std::max (20, (bw - fixed - gaps * 8) / flex) : 0;
                int x = 16;
                for (size_t i = 0; i < r.items.size(); ++i)
                {
                    const int w = r.widths[i] > 0 ? r.widths[i] : share;
                    r.items[i]->setBounds (x, y, w, h);
                    x += w + 8;
                }
            }
            y += h + r.gap;
        }
        return y;
    }
    std::vector<std::unique_ptr<Component>> owned;
    std::vector<Row> rows;
    std::vector<PButton*> footers;
};

// ---- Modal ------------------------------------------------------------------------------------------
Modal::Modal (DialogHost& h) : host (h) { setVisible (false); setWantsKeyboardFocus (true); }
Modal::~Modal() = default;

void Modal::later (std::function<void()> fn)
{
    Component::SafePointer<Modal> sp (this);
    MessageManager::callAsync ([sp, fn = std::move (fn)] { if (sp != nullptr) fn(); });
}

void Modal::paint (Graphics& g) { g.fillAll (Colours::black.withAlpha (0.6f)); }
void Modal::resized() { if (panel) panel->setCentrePosition (getWidth() / 2, getHeight() / 2); }
void Modal::mouseDown (const MouseEvent&) { close(); }
bool Modal::keyPressed (const KeyPress& k)
{
    if (k.isKeyCode (KeyPress::escapeKey)) { close(); return true; }
    return true; // swallow keys while a dialog is open
}
void Modal::close()
{
    setVisible (false);
    panel.reset();
    if (auto* p = getParentComponent()) p->grabKeyboardFocus();
}
void Modal::show (std::unique_ptr<Panel> p)
{
    panel = std::move (p);
    addAndMakeVisible (*panel);
    setBounds (getParentComponent()->getLocalBounds());
    setVisible (true);
    toFront (true);
    resized();
    grabKeyboardFocus();
}

// ---- Settings, Guide, About -------------------------------------------------------------------------
void Modal::openSettings()
{
    auto p = std::make_unique<Panel> (*this, host, "Settings", 700);
    auto& c = host.context();
    auto& proc = host.processor();
    auto* tips = p->make<PToggle> (c, "Info bubbles after hovering for about one second", c.tips);
    tips->tip = "Shows a short explanation when you rest the pointer on any control. Try turning it off once you know the controls.";
    tips->onChange = [this, &proc] (bool v) { host.context().tips = v; proc.setSetting (setting::tips, v); host.settingsChanged(); };
    p->row ({ tips }, { 0 }, 24);
    auto* pw = p->make<PToggle> (c, "Power saving (redraw at 16 frames per second)", c.power);
    pw->tip = "Redraws the meters and displays less often to save CPU. Try it on a laptop battery.";
    pw->onChange = [this, &proc] (bool v) { host.context().power = v; proc.setSetting (setting::power, v); host.settingsChanged(); };
    p->row ({ pw }, { 0 }, 24);
    auto* nz = p->make<PToggle> (c, "Add the hardware noise floor and mains hum (off by default)", (bool) proc.getSetting (setting::noise, false));
    nz->tip = "Adds the hardware noise floor (spec: about 70 to 75 dB below +10 dBm, here about -78 dBFS) and a faint 60 Hz hum (our own level, the manual gives none). Try it with no input and the Gain high.";
    nz->onChange = [this, &proc] (bool v) { proc.setSetting (setting::noise, v); host.settingsChanged(); };
    p->row ({ nz }, { 0 }, 24, 10);

    auto* lookLabel = p->make<TextBlock> (c, std::vector<Seg> { { "Colour look" } });
    auto* combo = p->make<ComboBox>();
    combo->addItem ("Look C, charcoal and white (default)", 1);
    combo->addItem ("Look A, neon navy", 2);
    combo->addItem ("Look B, purple soda pop", 3);
    combo->setSelectedId (c.look == 'A' ? 2 : c.look == 'B' ? 3 : 1, dontSendNotification);
    combo->setTooltip ({});
    combo->onChange = [this, combo, &proc]
    {
        const char ch = combo->getSelectedId() == 2 ? 'A' : combo->getSelectedId() == 3 ? 'B' : 'C';
        host.context().look = ch;
        proc.setSetting (setting::look, String::charToString ((juce_wchar) ch));
        host.settingsChanged();
    };
    p->row ({ lookLabel, combo }, { 90, 340 }, 28);
    p->textRow ({ { "The look is saved with the plugin settings, not with each preset. Wood, cat logo, meter colours and dB scales stay the same in all three.", false, false, true } }, 12);

    auto* reset = p->make<PButton> (c, "Reset window to 100%");
    reset->onClick = [this] (const ModifierKeys&) { host.resetWindow(); host.toast ("Window reset to 100%"); };
    p->row ({ reset }, { 190 }, 28);
    p->fit();
    show (std::move (p));
}

void Modal::openGuide (int tab)
{
    auto p = std::make_unique<Panel> (*this, host, "Guide", 900);
    auto& c = host.context();
    const char* names[3] = { "Basics", "Thicken a supersaw", "Model: verified and tuned" };
    std::vector<Component*> chips; std::vector<int> cw;
    for (int i = 0; i < 3; ++i)
    {
        auto* b = p->make<PButton> (c, names[i], 13.0f);
        b->radius = 12.0f; b->spacing = 0.0f; b->on = i == tab;
        b->onClick = [this, i] (const ModifierKeys&) { later ([this, i] { openGuide (i); }); };
        chips.push_back (b); cw.push_back ((int) textWidth (theme::font (theme::FontKind::regular, 13.0f), names[i]) + 24);
    }
    p->row (chips, cw, 24, 6);
    const float px = 13.5f;
    auto text = [&] (std::vector<Seg> s, int gap = 5) { p->textRow (std::move (s), gap); };
    (void) text;

    if (tab == 0)
    {
        p->textRow ({ { "Peak Reduction", true }, { " sets how hard the cell is driven: a threshold from 0 down to -40 dB. " }, { "Gain", true },
                      { " is make-up after the cell, so it never changes the compression or where the meter sits; it makes the sound louder and pushes the tube stage harder. As in the real unit the side-chain listens to the signal after the cell (feedback), which gives the soft knee. Flow: input, sidechain filter, T4 cell, Gain, tube stage, output transformer, Mix, output." } }, 6);
        const std::pair<const char*, const char*> keys[] = {
            { "B", "Bypass on or off, to compare with the dry sound" },
            { "C", "Switch Compress and Limit" },
            { "M", "Cycle the meter: GR, +10, +4" },
            { "Knob: drag, wheel, arrows", "Hold Shift for fine steps, double-click to reset" },
            { "Displays", "Drag the transfer curve sideways for Peak Reduction; drag the sidechain filter up or down for HF Emphasis" },
            { "Wood cheeks", "Double-click to reset the window to 100%" } };
        for (auto& k : keys)
        {
            auto* kb = p->make<Kbd> (c, k.first);
            auto* tb = p->make<TextBlock> (c, std::vector<Seg> { { k.second } }, px);
            p->kvRow (kb, tb, 190);
        }
        p->textRow ({ { "Keys work while the plugin window has keyboard focus.", false, false, true } }, 6);
        p->textRow ({ { "Where to start:", true }, { " load Start Here and play your audio, then raise Peak Reduction until the needle shows 3 to 6 dB on the loud parts. Lift Gain until the level matches the dry sound (Bypass, key B). Match the level before you judge: louder always sounds better." } }, 6);
        p->textRow ({ { "The cell:", true }, { " it lets go fast at first (about 60 ms to half of the reduction) and then slowly, from a fraction of a second up to several seconds after long or deep compression. The needle has VU style ballistics, so very short hits show as a pulse, and the history display shows every hit. If the needle sits at 0, the signal is below the threshold: raise Peak Reduction. Gain can never move it." } }, 6);
    }
    else if (tab == 1)
    {
        p->textRow ({ { "The LA-2A is a favourite for thickening supersaw leads and pads. Three things happen together. " }, { "1.", true },
                      { " The T4 cell turns loud parts down slowly and with memory, so sustained notes get a dense, even body. " }, { "2.", true },
                      { " The cell itself adds a little third harmonic that grows with gain reduction. " }, { "3.", true },
                      { " Gain pushes the tube stage and the output transformer, which add mild low-order harmonics. Seven detuned saws beat against these harmonics and each other, and you hear that as thickness. Note: published measurements of real units show mostly third harmonic with very little second, so our even-order bias is a small tuning choice, not a measured fact." } }, 6);
        // preset table
        auto rowsData = std::make_shared<std::vector<Preset>>();
        for (auto& q : host.store().all()) if (q.tags.contains ("thicken")) rowsData->push_back (q);
        const int colX[7] = { 0, 175, 222, 269, 337, 385, 806 };
        const int bodyW = 868;
        int total = 22;
        std::vector<int> heights;
        for (auto& q : *rowsData)
        {
            const String desc = (q.src.isNotEmpty() ? q.src + ": " : String()) + q.note;
            TextLayout tl; AttributedString as; as.setWordWrap (AttributedString::byWord); as.setLineSpacing (2.0f);
            as.append (desc, theme::font (theme::FontKind::regular, px), Colours::white);
            tl.createLayout (as, (float) (806 - 385 - 12));
            const int h = std::max (30, (int) std::ceil (tl.getHeight()) + 8);
            heights.push_back (h); total += h;
        }
        struct Table : Component
        {
            Table (DialogHost& h, std::shared_ptr<std::vector<Preset>> r, std::vector<int> hs, Modal& m, float px_) : host (h), data (std::move (r)), heights (std::move (hs)), modal (m), size (px_) {}
            void paint (Graphics& g) override
            {
                auto& t = host.context().t();
                auto f = theme::font (theme::FontKind::regular, size), fb = theme::font (theme::FontKind::bold, size);
                const int X[7] = { 0, 175, 222, 269, 337, 385, 806 };
                const char* hd[6] = { "Preset", "Peak", "Gain", "Ratio", "Mix", "Source and what you hear" };
                for (int i = 0; i < 6; ++i) drawText (g, hd[i], { (float) X[i] + 8, 0, 300, 22 }, fb, t.ink, Justification::centredLeft);
                g.setColour (t.line); g.fillRect (0, 21, getWidth(), 1);
                int y = 22;
                for (size_t k = 0; k < data->size(); ++k)
                {
                    auto& q = (*data)[k];
                    const int h = heights[k];
                    const String desc = (q.src.isNotEmpty() ? q.src + ": " : String()) + q.note;
                    const String cells[5] = { q.name, String (roundToInt (q.v[0])), String (roundToInt (q.v[1])), q.v[2] > 0.5 ? "Limit" : "Compress", String (roundToInt (q.v[5])) + "%" };
                    for (int i = 0; i < 5; ++i) drawText (g, cells[i], { (float) X[i] + 8, (float) y, (float) (X[i + 1] - X[i]), 28 }, f, t.ink, Justification::centredLeft);
                    AttributedString as; as.setWordWrap (AttributedString::byWord); as.setLineSpacing (2.0f);
                    as.append (desc, f, t.ink);
                    as.draw (g, Rectangle<float> ((float) X[5] + 8, (float) y + 4, (float) (X[6] - X[5] - 12), (float) h - 4));
                    g.setColour (t.line); g.fillRect (0, y + h - 1, getWidth(), 1);
                    y += h;
                }
            }
            DialogHost& host; std::shared_ptr<std::vector<Preset>> data; std::vector<int> heights; Modal& modal; float size;
        };
        auto* table = p->make<Table> (host, rowsData, heights, *this, px);
        int y = 22;
        for (size_t k = 0; k < rowsData->size(); ++k)
        {
            auto* lb = p->make<PButton> (c, "Load", 13.0f);
            lb->tip = "Loads this preset. Then play your supersaw and use Bypass (key B) to hear the difference.";
            const Preset q = (*rowsData)[k];
            lb->onClick = [this, q] (const ModifierKeys&) { later ([this, q] { close(); host.loadPreset (q); host.toast ("Loaded " + q.name); }); };
            table->addAndMakeVisible (lb);
            lb->setBounds (colX[6], y + 3, 56, 24);
            y += heights[k];
        }
        (void) bodyW;
        p->row ({ table }, { 0 }, total, 8);
        p->textRow ({ { "Method:", true }, { " play a supersaw lead, raise Peak Reduction until the needle shows 4 to 8 dB, then raise Gain until the loudness matches Bypass (key B). Push both a little further and match again. Stop when the top starts to fizz or the notes begin to pump." } }, 4);
    }
    else
    {
        p->textRow ({ { "What this is:", true }, { " a behaviour model built from published measurements of the cell, tube stage and transformer. It is " }, { "not", true }, { " a circuit-level simulation." } }, 6);
        auto bullets = [] (std::vector<String> l) { String s; for (auto& x : l) s += String::fromUTF8 ("\xe2\x80\xa2 ") + x + "\n"; return s.trimEnd(); };
        auto* left = p->make<TextBlock> (c, std::vector<Seg> {
            { "Verified from sources\n", true, false, false, false, 1 },
            { bullets ({
                "Peak Reduction is side-chain drive: threshold 0 to -40 dB. Gain is up to 40 dB and does not change the compression (UA manual).",
                "Side-chain is fed after the cell (feedback). It has a pre-emphasis filter and an adjustable high-frequency compression control (UA manual).",
                "Attack about 10 ms. Release about 60 ms to 50 percent, then 0.5 to 5 s (UA); Wikipedia says 1 to 15 s. Slower after long or deep compression (UA).",
                "Compress is about 3 to 1; Limit is much higher, about infinity in the UA wording.",
                "Tubes: 12AX7 stages, 12BH7 cathode follower, output transformer. THD under 0.35 percent at +10 dBm and 0.75 percent at +16 dBm; noise 70 to 75 dB below +10 dBm (UA manual).",
                "Six measured units (University of Huddersfield paper): at about 6 dB reduction THD was 0.9 to 4.2 percent, mostly third harmonic, second 50 dB or more down. Most of it comes from the cell, then the transformers. Attack and release vary a lot between units." }) } }, px);
        auto* right = p->make<TextBlock> (c, std::vector<Seg> {
            { "Our own tuning\n", true, false, false, false, 2 },
            { bullets ({
                "The two-stage cell: fast stage half-life about 42 ms, slow stage 0.35 to 4 s set by a memory of recent drive. A shape that fits the published numbers; it is not measured circuit data.",
                "Knee shapes and both ratio curves: Compress about 2.6 to 1 across 20 dB over the threshold; Limit about 4 to 1 near the knee rising to about 9 to 1 for big overshoots, gentler than the manual's \"about infinity\".",
                "Cell distortion strength, the small even-order tube bias, the transformer low-frequency saturation, 4x oversampling.",
                "HF Emphasis: the corner (about 500 Hz) and range. The manual gives none. It is a knob here, not a fixed filter.",
                "Digital levels for the +4 and +10 meter settings, VU style needle, hum level." }) + "\n" },
            { "Gaps\n", true, false, false, false, 3 },
            { bullets ({ "No circuit solve, no input transformer, no temperature or ageing, no unit to unit variation, no real CdS curve, no cell memory beyond minutes." }) } }, px);
        const int colW = (900 - 32 - 14) / 2;
        const int hh = std::max (left->heightFor (colW), right->heightFor (colW));
        p->row ({ left, right }, { colW, colW }, hh, 6);
    }
    p->fit();
    show (std::move (p));
}

void Modal::openAbout()
{
    auto p = std::make_unique<Panel> (*this, host, "About & Support", 700);
    auto& c = host.context();
    p->textRow ({ { "LapCat2A", true }, { String (" by Studio 87, version ") + JucePlugin_VersionString + ". A free open source optical leveling amplifier inspired by the classic LA-2A, built from published manuals and measurements. It is a behaviour model, not a circuit simulation. Licence AGPL-3.0-or-later. Collects nothing and never uses the network." } });
    p->textRow ({ { "Your presets are saved here:", true }, { "\nLinux  " }, { "~/.local/share/Studio 87/LapCat2A/presets", false, true },
                  { "\nWindows  " }, { "%APPDATA%\\Studio 87\\LapCat2A\\presets", false, true },
                  { "\nmacOS  " }, { "~/Library/Application Support/Studio 87/LapCat2A/presets", false, true } });
    p->textRow ({ { "Install and uninstall:", true }, { " the installer puts the VST3 in the standard folder for your system. The uninstaller removes everything it installed and asks before touching your presets (it keeps them by default)." } });
    p->textRow ({ { "Support is entirely optional. Nothing is locked. The buttons open your browser." } }, 10);
    auto* gh = p->make<PButton> (c, "GitHub");
    auto* rel = p->make<PButton> (c, "Releases");
    auto* kofi = p->make<PButton> (c, "Ko-fi studio87");
    gh->onClick = [] (const ModifierKeys&) { URL ("https://github.com/SpeakingColors/LapCat2A").launchInDefaultBrowser(); };
    rel->onClick = [] (const ModifierKeys&) { URL ("https://github.com/SpeakingColors/LapCat2A/releases").launchInDefaultBrowser(); };
    kofi->onClick = [] (const ModifierKeys&) { URL ("https://ko-fi.com/studio87").launchInDefaultBrowser(); };
    // Studio 87 Gear Updater link: asks the OS to open studio87://open?product=LapCat2A. No network call by the plugin; nothing is sent.
    auto* upd = p->make<PButton> (c, studio87link::buttonText());
    upd->tip = studio87link::tipText();
    auto* updMsg = p->make<TextBlock> (c, std::vector<Seg> { { String(), false, false, true } }, 13.0f);
    upd->onClick = [updMsg] (const ModifierKeys&)
    {
        if (! URL (String (studio87link::updaterUrl ("LapCat2A"))).launchInDefaultBrowser())
        {
            studio87link::openGitHub ("LapCat2A");   // no handler for studio87://: open the plugin's GitHub home page instead
            updMsg->setSegs ({ { studio87link::fallbackNote(), false, false, true } });
        }
    };
    auto* ghb = p->make<PButton> (c, studio87link::githubButtonText());
    ghb->tip = studio87link::githubTipText();
    ghb->onClick = [] (const ModifierKeys&) { studio87link::openGitHub ("LapCat2A"); };
    p->row ({ gh, rel, kofi }, { 90, 90, 140 }, 28);
    p->row ({ upd, ghb }, { 240, 170 }, 28);
    p->row ({ updMsg }, { 0 }, 36);
    p->fit();
    show (std::move (p));
}

// ---- Preset browser (pages of 8, no scrolling) ------------------------------------------------------
namespace
{
String pbSel, pbType = "All", pbQuery;
int pbPage = 0;

class ItemRow : public Component, public TipClient
{
public:
    ItemRow (DialogHost& h) : host (h) { setMouseCursor (MouseCursor::PointingHandCursor); }
    Preset p; bool selected = false;
    std::function<void()> onSelect, onLoad;
    void paint (Graphics& g) override
    {
        auto& t = host.context().t();
        if (selected) { g.setColour (theme::sel); g.fillRect (getLocalBounds()); }
        g.setColour (t.line); g.fillRect (0, getHeight() - 1, getWidth(), 1);
        drawText (g, p.name, { 10.0f, 0.0f, (float) getWidth() - 110.0f, (float) getHeight() }, theme::font (theme::FontKind::regular, 15.0f), selected ? theme::lilac : t.ink, Justification::centredLeft);
        drawText (g, (p.factory ? String() : String ("mine, ")) + p.type, { (float) getWidth() - 150.0f, 0.0f, 140.0f, (float) getHeight() },
                  theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredRight, 1.0f);
    }
    void mouseUp (const MouseEvent& e) override { if (e.mouseWasClicked() && onSelect) onSelect(); }
    void mouseDoubleClick (const MouseEvent&) override { if (onLoad) onLoad(); }
private:
    DialogHost& host;
};

class BrowserBody : public Component
{
public:
    static constexpr int kPer = 8, kRowH = 30;
    BrowserBody (Modal& m, DialogHost& h) : modal (m), host (h)
    {
        auto& c = h.context();
        styleEditor (search, c, false);
        search.setName ("pbQ");
        search.setTextToShowWhenEmpty ("Search name, type or tag", c.t().dim);
        search.setText (pbQuery, false);
        search.onTextChange = [this] { pbQuery = search.getText(); pbPage = 0; refresh(); };
        search.onEscapeKey = [this] { modal.close(); };
        addAndMakeVisible (search);
        const char* all[] = { "All", "Vocal", "Bass", "Drum", "Keys", "Guitar", "Synth", "Bus", "FX" };
        for (auto* n : all)
        {
            auto* b = chips.add (new PButton (c, n, 13.0f));
            b->radius = 12.0f; b->spacing = 0.0f;
            b->onClick = [this, n] (const ModifierKeys&) { pbType = n; pbPage = 0; refresh(); };
            addAndMakeVisible (b);
        }
        for (int i = 0; i < kPer; ++i)
        {
            auto* r = rowsUi.add (new ItemRow (h));
            r->tip = "Click to select this preset; double-click to load it.";
            addAndMakeVisible (r);
        }
        prev = new PButton (c, String::fromUTF8 ("\xe2\x97\x80")); prev->setName ("<"); prev->spacing = 0.0f;
        next = new PButton (c, String::fromUTF8 ("\xe2\x96\xb6")); next->setName (">"); next->spacing = 0.0f;
        pagers.add (prev); pagers.add (next);
        addAndMakeVisible (prev); addAndMakeVisible (next);
        prev->onClick = [this] (const ModifierKeys&) { pbPage = std::max (0, pbPage - 1); refresh(); };
        next->onClick = [this] (const ModifierKeys&) { ++pbPage; refresh(); };
        for (auto* n : { "Load", "Copy and rename", "Rename", "Delete", "Export" })
        {
            auto* b = sideBtns.add (new PButton (c, n));
            addAndMakeVisible (b);
        }
        sideBtns[0]->on = true;
        sideBtns[0]->onClick = [this] (const ModifierKeys&) { if (auto* p = selected()) { Preset q = *p; modal.later ([this, q] { modal.close(); host.loadPreset (q); }); } };
        sideBtns[1]->onClick = [this] (const ModifierKeys&) { if (auto* p = selected()) { Preset q = *p; modal.later ([this, q] { modal.openSave (&q); }); } };
        sideBtns[2]->onClick = sideBtns[1]->onClick;
        sideBtns[3]->onClick = [this] (const ModifierKeys&)
        {
            if (auto* p = selected())
            {
                const String n = p->name;
                host.store().remove (n);
                if (host.presetName() == n) host.setPresetName (presets::factory()[0].name);
                host.toast ("Deleted " + n);
                pbSel = {};
                refresh();
            }
        };
        sideBtns[4]->onClick = [this] (const ModifierKeys&) { if (auto* p = selected()) { Preset q = *p; modal.later ([this, q] { modal.openExport (&q); }); } };
        refresh();
    }

    const Preset* selected() const { return host.store().find (pbSel); }

    void refresh()
    {
        for (auto* b : chips) b->setOn (b->text == pbType);
        shown.clear();
        const String q = pbQuery.toLowerCase();
        for (auto& p : host.store().all())
        {
            if (pbType != "All" && p.type != pbType) continue;
            if (q.isNotEmpty() && ! (p.name + " " + p.type + " " + p.tags.joinIntoString (" ")).toLowerCase().contains (q)) continue;
            shown.push_back (p);
        }
        pages = std::max (1, ((int) shown.size() + kPer - 1) / kPer);
        pbPage = jlimit (0, pages - 1, pbPage);
        for (int i = 0; i < kPer; ++i)
        {
            auto* r = rowsUi[i];
            const int k = pbPage * kPer + i;
            r->setVisible (k < (int) shown.size());
            if (k < (int) shown.size())
            {
                r->p = shown[(size_t) k];
                r->selected = r->p.name == pbSel;
                const Preset pp = r->p;
                r->onSelect = [this, pp] { pbSel = pp.name; refresh(); };
                r->onLoad = [this, pp] { modal.later ([this, pp] { modal.close(); host.loadPreset (pp); }); };
                r->repaint();
            }
        }
        const Preset* s = selected();
        sideBtns[0]->setVisible (s != nullptr);
        sideBtns[1]->setVisible (s != nullptr && s->factory);
        sideBtns[2]->setVisible (s != nullptr && ! s->factory);
        sideBtns[3]->setVisible (s != nullptr && ! s->factory);
        sideBtns[4]->setVisible (s != nullptr);
        resized();
        repaint();
        autoTip (*this);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto left = r.removeFromLeft (290);
        search.setBounds (left.removeFromTop (30));
        left.removeFromTop (6);
        int x = 0, y = left.getY();
        for (auto* b : chips)
        {
            const int w = (int) textWidth (theme::font (theme::FontKind::regular, 13.0f), b->text) + 20;
            if (x + w > left.getWidth()) { x = 0; y += 26; }
            b->setBounds (left.getX() + x, y, w, 22);
            x += w + 4;
        }
        left.setTop (y + 30);
        listArea = left;
        for (int i = 0; i < kPer; ++i) rowsUi[i]->setBounds (listArea.getX() + 1, listArea.getY() + 1 + i * kRowH, listArea.getWidth() - 2, kRowH);
        const int py = listArea.getBottom() - 32;
        prev->setBounds (listArea.getCentreX() - 90, py + 3, 34, 26);
        next->setBounds (listArea.getCentreX() + 56, py + 3, 34, 26);
        r.removeFromLeft (14);
        sideArea = r;
        int bx = r.getX(), by = r.getY() + 198;
        for (auto* b : sideBtns)
        {
            if (! b->isVisible()) continue;
            const int w = (int) textWidth (theme::font (theme::FontKind::regular, 14.0f), b->text) + 28;
            if (bx + w > r.getRight()) { bx = r.getX(); by += 34; }
            b->setBounds (bx, by, w, 28);
            bx += w + 6;
        }
    }

    void paint (Graphics& g) override
    {
        auto& t = host.context().t();
        g.setColour (t.disp); g.fillRect (listArea);
        g.setColour (t.line); g.drawRect (listArea, 1);
        if (shown.empty()) drawText (g, "No match", listArea.withHeight (kRowH).toFloat().withTrimmedLeft (10), theme::font (theme::FontKind::regular, 15.0f), t.ink, Justification::centredLeft);
        const int py = listArea.getBottom() - 32;
        g.setColour (t.line); g.fillRect (listArea.getX() + 1, py, listArea.getWidth() - 2, 1);
        drawText (g, "Page " + String (pbPage + 1) + " of " + String (pages), { (float) listArea.getCentreX() - 52.0f, (float) py, 104.0f, 32.0f },
                  theme::font (theme::FontKind::regular, 15.0f), t.ink);
        const Preset* p = selected();
        if (p == nullptr) return;
        auto r = sideArea.toFloat();
        drawText (g, p->name, r.withHeight (28.0f), theme::font (theme::FontKind::regular, 20.0f), t.title, Justification::centredLeft);
        const String sep = String::fromUTF8 (" \xc2\xb7 ");
        drawText (g, p->type + (p->tags.isEmpty() ? String() : sep + p->tags.joinIntoString (", ")), r.withTrimmedTop (30.0f).withHeight (20.0f),
                  theme::font (theme::FontKind::regular, 15.0f), t.dim, Justification::centredLeft);
        const char* labels[5] = { "Peak Reduction", "Gain", "Mode", "HF Emphasis", "Mix" };
        const String vals[5] = { String (p->v[0]), String (p->v[1]), p->v[2] > 0.5 ? "Limit" : "Compress", String (p->v[4]), String (p->v[5]) + "%" };
        float y = r.getY() + 62.0f;
        for (int i = 0; i < 5; ++i)
        {
            drawText (g, labels[i], { r.getX() + 8, y, 130.0f, 24.0f }, theme::font (theme::FontKind::regular, 15.0f), t.ink, Justification::centredLeft);
            drawText (g, vals[i], { r.getX() + 150, y, 120.0f, 24.0f }, theme::font (theme::FontKind::regular, 15.0f), t.ink, Justification::centredLeft);
            g.setColour (t.line); g.fillRect (r.getX(), y + 24.0f, r.getWidth(), 1.0f);
            y += 25.0f;
        }
    }

    Modal& modal;
    DialogHost& host;
    TipEditor search;
    OwnedArray<PButton> chips, sideBtns;
    OwnedArray<ItemRow> rowsUi;
    OwnedArray<PButton> pagers;
    PButton *prev = nullptr, *next = nullptr;
    std::vector<Preset> shown;
    int pages = 1;
    Rectangle<int> sideArea, listArea;
};
} // namespace

void Modal::openBrowser()
{
    auto p = std::make_unique<Panel> (*this, host, "Presets", 760);
    if (pbSel.isEmpty()) pbSel = host.presetName();
    auto* body = p->make<BrowserBody> (*this, host);
    p->row ({ body }, { 0 }, 372);
    p->footer ("Import", [this] { openImport(); });
    p->footer ("Export all", [this] { openExport (nullptr); });
    p->footer ("Done", [this] { close(); });
    p->fit();
    show (std::move (p));
}

// ---- Save, Export, Import ---------------------------------------------------------------------------
void Modal::openSave (const Preset* edit)
{
    const bool hasEdit = edit != nullptr;
    auto p = std::make_unique<Panel> (*this, host, hasEdit ? "Rename or edit" : "Save preset", 700);
    auto& c = host.context();
    const Preset editCopy = hasEdit ? *edit : Preset();
    const Preset* curP = host.store().find (host.presetName());
    if (curP == nullptr) curP = &presets::factory()[0];

    auto* nameLabel = p->make<TextBlock> (c, std::vector<Seg> { { "Name" } });
    auto* nameBox = p->make<TipEditor>();
    nameBox->setName ("svN");
    styleEditor (*nameBox, c, false);
    nameBox->setInputRestrictions (60);
    String startName = hasEdit ? (editCopy.factory ? editCopy.name + " mine" : editCopy.name) : (curP->factory ? curP->name + " mine" : curP->name);
    nameBox->setText (startName, false);
    nameBox->onEscapeKey = [this] { close(); };
    p->row ({ nameLabel, nameBox }, { 50, 0 }, 28);

    auto* typeLabel = p->make<TextBlock> (c, std::vector<Seg> { { "Type" } });
    auto typeSel = std::make_shared<String> (hasEdit ? editCopy.type : curP->type);
    auto chipList = std::make_shared<std::vector<PButton*>>();
    for (auto* ty : presets::types)
    {
        auto* b = p->make<PButton> (c, ty, 13.0f);
        b->radius = 12.0f; b->spacing = 0.0f;
        b->on = *typeSel == ty;
        b->tip = "Sets the type, used for filtering in the preset list. Try the one that matches the sound.";
        chipList->push_back (b);
    }
    for (auto* b : *chipList)
        b->onClick = [typeSel, chipList, b] (const ModifierKeys&) { *typeSel = b->text; for (auto* x : *chipList) x->setOn (x == b); };
    {
        std::vector<Component*> items { typeLabel }; std::vector<int> widths { 50 };
        for (auto* b : *chipList) { items.push_back (b); widths.push_back (0); }
        p->row (items, widths, 26);
    }

    auto* tagLabel = p->make<TextBlock> (c, std::vector<Seg> { { "Tags" } });
    auto* tagBox = p->make<TipEditor>();
    tagBox->setName ("svG");
    styleEditor (*tagBox, c, false);
    tagBox->setTextToShowWhenEmpty ("comma separated", c.t().dim);
    tagBox->setText ((hasEdit ? editCopy.tags : curP->tags).joinIntoString (", "), false);
    tagBox->onEscapeKey = [this] { close(); };
    p->row ({ tagLabel, tagBox }, { 50, 0 }, 28);

    auto* warnText = p->make<Label>();
    warnText->setColour (Label::textColourId, theme::red);
    warnText->setFont (theme::font (theme::FontKind::regular, 15.0f));
    auto* overwrite = p->make<PButton> (c, "Overwrite");
    auto* asNew = p->make<PButton> (c, "Save as new");
    overwrite->setVisible (false); asNew->setVisible (false);
    p->row ({ warnText, overwrite, asNew }, { 0, 90, 100 }, 28);

    p->footer ("Cancel", [this] { close(); });
    auto saveFn = [this, hasEdit, editCopy, nameBox, tagBox, typeSel, warnText, overwrite, asNew]
    {
        const String name = nameBox->getText().trim();
        overwrite->setVisible (false); asNew->setVisible (false);
        auto commit = [this, hasEdit, editCopy, tagBox, typeSel, warnText] (const String& n)
        {
            StringArray tags;
            tags.addTokens (tagBox->getText(), ",", "");
            tags.trim(); tags.removeEmptyStrings();
            while (tags.size() > 8) tags.remove (tags.size() - 1);
            for (auto& t : tags) t = t.substring (0, 20);
            Preset q = hasEdit ? editCopy : host.currentValues();
            q.name = n; q.type = *typeSel; q.tags = tags; q.factory = false; q.src = {}; q.note = {};
            if (host.store().save (q, (hasEdit && ! editCopy.factory) ? editCopy.name : String()))
            {
                if (hasEdit) host.loadPreset (q); // like the mockup: the saved copy becomes the loaded preset
                host.setPresetName (n);
                pbSel = n;
                close();
                host.toast ("Saved " + n);
            }
            else warnText->setText ("Could not write the preset file. Check that your presets folder is writable.", dontSendNotification);
        };
        if (name.isEmpty()) { warnText->setText ("Give it a name first.", dontSendNotification); return; }
        for (auto& f : presets::factory())
            if (f.name == name) { warnText->setText ("That is a factory name. Pick another; factory sounds stay as they are.", dontSendNotification); return; }
        bool clash = false;
        for (auto& u : host.store().user()) if (u.name == name && ! (hasEdit && ! editCopy.factory && u.name == editCopy.name)) clash = true;
        if (clash)
        {
            warnText->setText ("A preset called " + name + " already exists. Overwrite it, save as a new one, or cancel.", dontSendNotification);
            overwrite->setVisible (true); asNew->setVisible (true);
            overwrite->onClick = [commit, name] (const ModifierKeys&) { commit (name); };
            asNew->onClick = [this, commit, name] (const ModifierKeys&)
            {
                int n = 2; String nn = name + " " + String (n);
                while (host.store().find (nn) != nullptr) nn = name + " " + String (++n);
                commit (nn);
            };
            return;
        }
        commit (name);
    };
    p->footer ("Save", saveFn, true);
    p->fit();
    show (std::move (p));
    nameBox->grabKeyboardFocus();
}

void Modal::openExport (const Preset* one)
{
    auto p = std::make_unique<Panel> (*this, host, "Export", 700);
    auto& c = host.context();
    p->textRow ({ { "Copy this text and keep it. Import it later or on another computer." } });
    auto* box = p->make<TipEditor>();
    box->setName ("exT");
    styleEditor (*box, c, true);
    box->setReadOnly (true);
    std::vector<Preset> list;
    if (one != nullptr) list.push_back (*one); else list = host.store().user();
    box->setText (host.store().exportJson (list), false);
    p->row ({ box }, { 0 }, 230);
    p->footer ("Copy", [this, box] { SystemClipboard::copyTextToClipboard (box->getText()); host.toast ("Copied"); });
    p->footer ("Done", [this] { close(); });
    p->fit();
    show (std::move (p));
}

void Modal::openImport()
{
    auto p = std::make_unique<Panel> (*this, host, "Import", 700);
    auto& c = host.context();
    p->textRow ({ { "Paste exported preset text here. Unknown settings are ignored and anything invalid is refused." } });
    auto* box = p->make<TipEditor>();
    box->setName ("imT");
    styleEditor (*box, c, true);
    p->row ({ box }, { 0 }, 200);
    auto* warn = p->make<Label>();
    warn->setColour (Label::textColourId, theme::red);
    warn->setFont (theme::font (theme::FontKind::regular, 15.0f));
    p->row ({ warn }, { 0 }, 24);
    p->footer ("Cancel", [this] { close(); });
    p->footer ("Import", [this, box, warn]
    {
        const int n = host.store().importJson (box->getText());
        if (n < 0) { warn->setText ("That text is not a valid LapCat2A preset export. Nothing was imported.", dontSendNotification); return; }
        host.toast ("Imported " + String (n) + (n == 1 ? " preset" : " presets"));
        close();
    }, true);
    p->fit();
    show (std::move (p));
}
} // namespace ui
