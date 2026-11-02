#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Preset values use the real parameter units: peak 0..100, gain 0..100, mode 0/1 (Compress, Limit),
// meter 0..2 (GR, +10, +4), emph 0..100, mix 0..100, bypass 0/1. Unknown keys are ignored and out of range
// values refuse the preset. Loading a preset sets peak, gain, mode, emph and mix; the meter choice and
// bypass belong to the listening session, so loading a preset leaves them as they are.
struct Preset
{
    juce::String name, type = "Bus";
    juce::StringArray tags;
    bool factory = false;
    double v[7] { 35, 20, 0, 0, 0, 100, 0 };
    juce::String src, note; // factory only: the Guide's "Thicken a supersaw" table
};

namespace presets
{
inline constexpr int kCount = 7;
inline const char* const keys[kCount] = { "peak", "gain", "mode", "meter", "emph", "mix", "bypass" };
inline const double lo[kCount] = { 0, 0, 0, 0, 0, 0, 0 };
inline const double hi[kCount] = { 100, 100, 1, 2, 100, 100, 1 };
inline const char* const types[] = { "Vocal", "Bass", "Drum", "Keys", "Guitar", "Synth", "Bus", "FX" };
inline constexpr int kNumTypes = 8;

const std::vector<Preset>& factory();
juce::File userFolder();

class Store
{
public:
    Store();
    void reload();                                    // read the user folder
    std::vector<Preset> all() const;                  // factory first, then yours
    const std::vector<Preset>& user() const { return mine; }
    const Preset* find (const juce::String& name) const;
    bool save (const Preset& p, const juce::String& replaceName = {}); // writes the file, replaces a same-named one
    bool remove (const juce::String& name);
    juce::String exportJson (const std::vector<Preset>& list) const;
    int importJson (const juce::String& text);        // returns count, or -1 when invalid
private:
    std::vector<Preset> mine;
};

bool valid (const juce::var& presetObject);
Preset fromVar (const juce::var& o);
juce::var toVar (const Preset& p);
} // namespace presets
