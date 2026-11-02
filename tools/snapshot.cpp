// Renders the real editor offscreen to a PNG so it can be compared with the mockup, and runs plugin self tests.
// usage: LapSnapshot out.png [scale] [look A|B|C] [dialog none|settings|guide|guide2|guide3|about|presets|save|export|import|tip] [frames] [preset name] [meter 0|1|2]
//        LapSnapshot --selftest [fixture.xml]
//        LapSnapshot --write-fixture out.xml
#include "../src/plugin/PluginProcessor.h"
#include "../src/editor/PluginEditor.h"
#include "../src/editor/Presets.h"

static void setReal (LapCat2AProcessor& a, const char* id, float real)
{
    auto* p = a.apvts.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (real));
}

static void setFixtureValues (LapCat2AProcessor& a)
{
    setReal (a, ids::peak, 62.5f); setReal (a, ids::gain, 33.0f); setReal (a, ids::mode, 1); setReal (a, ids::meter, 2);
    setReal (a, ids::emph, 25.0f); setReal (a, ids::mix, 40.0f); setReal (a, ids::bypass, 0);
    a.setSetting (setting::look, "B"); a.setSetting (setting::tips, false); a.setSetting (setting::power, true);
    a.setSetting (setting::noise, true); a.setSetting (setting::preset, "Hoover Leveler");
}

// Saved state of state version 1 must keep loading (tests/fixtures/state-v1.xml).
static int fixtureTest (const juce::File& f)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    LapCat2AProcessor p;
    auto xml = juce::XmlDocument::parse (f);
    check (xml != nullptr, "fixture parses");
    if (xml == nullptr) return 1;
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (*xml, mb);
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    auto real = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    check (std::abs (real (ids::peak) - 62.5f) < 0.02f && std::abs (real (ids::gain) - 33.0f) < 0.02f && std::abs (real (ids::emph) - 25.0f) < 0.02f
               && std::abs (real (ids::mix) - 40.0f) < 0.02f, "knob values from the v1 fixture");
    check ((int) real (ids::mode) == 1 && (int) real (ids::meter) == 2 && real (ids::bypass) < 0.5f, "switch and choice values from the v1 fixture");
    check (p.getSetting (setting::look, "C").toString() == "B" && ! (bool) p.getSetting (setting::tips, true) && (bool) p.getSetting (setting::power, false)
               && (bool) p.getSetting (setting::noise, false) && p.getSetting (setting::preset, "").toString() == "Hoover Leveler",
           "settings from the v1 fixture");
    return bad;
}

static int selfTest (const juce::File& fixture)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    LapCat2AProcessor a, b;
    auto real = [] (LapCat2AProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    check (std::abs (real (a, ids::peak) - 50.0f) < 0.01f && std::abs (real (a, ids::gain) - 14.0f) < 0.01f, "defaults are the Start Here preset (50 and 14)");
    check ((int) real (a, ids::mode) == 0 && (int) real (a, ids::meter) == 0 && std::abs (real (a, ids::mix) - 100.0f) < 0.01f && real (a, ids::emph) < 0.01f, "other defaults");
    check (a.getLatencySamples() == 0, "latency is reported after prepareToPlay only");
    a.prepareToPlay (48000.0, 512);
    check (a.getLatencySamples() == 30, "reported latency is 30 samples");
    // every item of every choice survives a set and read (last item included)
    bool all = true;
    for (int i = 0; i < 2; ++i) { setReal (a, ids::mode, (float) i); all = all && (int) real (a, ids::mode) == i; }
    check (all, "both ratio modes");
    all = true;
    for (int i = 0; i < 3; ++i) { setReal (a, ids::meter, (float) i); all = all && (int) real (a, ids::meter) == i; }
    check (all, "all 3 meter items");
    // ranges
    setReal (a, ids::peak, 100.0f); setReal (a, ids::gain, 100.0f);
    check (std::abs (real (a, ids::peak) - 100.0f) < 0.001f && std::abs (real (a, ids::gain) - 100.0f) < 0.001f, "knob ranges reach 100");
    // state round trip including plugin settings
    setFixtureValues (a);
    juce::MemoryBlock mb; a.getStateInformation (mb);
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    check (std::abs (real (b, ids::peak) - 62.5f) < 0.02f && std::abs (real (b, ids::gain) - 33.0f) < 0.02f && (int) real (b, ids::mode) == 1
               && (int) real (b, ids::meter) == 2 && std::abs (real (b, ids::mix) - 40.0f) < 0.1f, "parameters survive a state round trip");
    check (b.getSetting (setting::look, "C").toString() == "B" && (bool) b.getSetting (setting::noise, false) && b.getSetting (setting::preset, "").toString() == "Hoover Leveler",
           "settings survive a state round trip");
    // audio: stereo and mono, no NaN, bypass bit exact after the delay
    {
        LapCat2AProcessor c;
        c.prepareToPlay (48000.0, 256);
        setReal (c, ids::bypass, 1.0f);
        juce::AudioBuffer<float> buf (2, 256), ref (2, 256);
        juce::MidiBuffer midi;
        juce::Random rnd (3);
        std::vector<float> fed, got;
        for (int blk = 0; blk < 60; ++blk)
        {
            for (int i = 0; i < 256; ++i) { const float v = rnd.nextFloat() * 1.6f - 0.8f; buf.setSample (0, i, v); buf.setSample (1, i, v); fed.push_back (v); }
            c.processBlock (buf, midi);
            for (int i = 0; i < 256; ++i) got.push_back (buf.getSample (0, i));
        }
        bool exact = true;
        for (size_t i = 3000; i < fed.size() - 30; ++i) exact = exact && got[i + 30] == fed[i];
        check (exact, "bypass returns the input bit exactly after 30 samples");
        setReal (c, ids::bypass, 0.0f); setReal (c, ids::peak, 100.0f); setReal (c, ids::gain, 100.0f); setReal (c, ids::mode, 1);
        bool finite = true;
        for (int blk = 0; blk < 40; ++blk)
        {
            for (int i = 0; i < 256; ++i) { const float v = rnd.nextFloat() * 4.0f - 2.0f; buf.setSample (0, i, v); buf.setSample (1, i, -v); }
            c.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) finite = finite && std::isfinite (buf.getSample (ch, i));
        }
        check (finite, "loud noise at the extreme settings stays finite");
        juce::AudioBuffer<float> mono (1, 256);
        mono.clear();
        c.processBlock (mono, midi);
        check (true, "a mono buffer is accepted");
    }
    // an editor can be made, resized and deleted
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (b.createEditor());
        ed->setSize (440, 238);
        ed->setSize (1650, 894);
        check (ed->getWidth() == 1650, "editor creates and resizes");
    }
    if (fixture != juce::File()) bad += fixtureTest (fixture);
    std::printf (bad == 0 ? "ALL SELF TESTS PASSED\n" : "SELF TESTS FAILED\n");
    return bad;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--write-fixture")
    {
        LapCat2AProcessor p; setFixtureValues (p);
        auto state = p.apvts.copyState(); state.setProperty ("stateVersion", 1, nullptr);
        if (auto xml = state.createXml()) juce::File (argv[2]).replaceWithText (xml->toString());
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--selftest")
        return selfTest (argc > 2 ? juce::File (argv[2]) : juce::File()) == 0 ? 0 : 1;
    LapCat2AProcessor proc;
    proc.prepareToPlay (48000.0, 800);
    if (argc > 3) proc.setSetting (setting::look, juce::String (argv[3]));
    const juce::String presetName = argc > 6 ? juce::String (argv[6]) : juce::String ("Thicken Lead Heavy");
    for (auto& f : presets::factory())
        if (f.name == presetName)
        {
            setReal (proc, ids::peak, (float) f.v[0]); setReal (proc, ids::gain, (float) f.v[1]); setReal (proc, ids::mode, (float) f.v[2]);
            setReal (proc, ids::emph, (float) f.v[4]); setReal (proc, ids::mix, (float) f.v[5]);
            proc.setSetting (setting::preset, f.name);
        }
    if (argc > 7) setReal (proc, ids::meter, (float) juce::String (argv[7]).getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    const double sc = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 1.0;
    ed->setSize (juce::roundToInt (LapCat2AEditor::kW * sc), juce::roundToInt (LapCat2AEditor::kH * sc));
    const juce::String dialog = argc > 4 ? juce::String (argv[4]) : juce::String ("none");
    const int frames = argc > 5 ? juce::String (argv[5]).getIntValue() : 150;
    if (auto* pe = dynamic_cast<LapCat2AEditor*> (ed.get())) pe->openDialogForTesting ("live");
    // Feed a supersaw-like lead in real time so the meters, curve dot, needle and history show something.
    juce::AudioBuffer<float> buf (2, 800);
    juce::MidiBuffer midi;
    long pos = 0;
    const double ratios[7] = { 0.986, 0.992, 0.996, 1.0, 1.004, 1.008, 1.014 };
    for (int f = 0; f < frames; ++f)
    {
        for (int i = 0; i < 800; ++i, ++pos)
        {
            const double t = (double) pos / 48000.0;
            const double note = std::fmod (t, 1.2), env = note < 0.9 ? 1.0 : std::max (0.0, 1.0 - (note - 0.9) / 0.05);
            double l = 0.0, r = 0.0;
            for (int k = 0; k < 7; ++k)
            {
                const double ph = std::fmod (t * 440.0 * ratios[k] + k * 0.137, 1.0);
                const double s = (2.0 * ph - 1.0) * 0.17 * env;
                l += s * (1.0 - k / 6.0); r += s * (k / 6.0);
            }
            const double tk = std::fmod (t, 0.5);
            const double kick = 0.5 * std::exp (-tk * 12.0) * std::sin (6.2831853 * (50.0 + 90.0 * std::exp (-tk * 40.0)) * tk);
            buf.setSample (0, i, (float) (l + kick)); buf.setSample (1, i, (float) (r + kick));
        }
        proc.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
    }
    if (dialog != "none") { if (auto* pe = dynamic_cast<LapCat2AEditor*> (ed.get())) pe->openDialogForTesting (dialog); }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    juce::Image img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
    juce::File out (argv[1]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    return 0;
}
