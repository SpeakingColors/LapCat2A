#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/LapCat2A.h"
#include <atomic>

// Stable parameter IDs. Never rename these (saved projects use them).
namespace ids
{
    inline constexpr const char* peak = "peak";       // Peak Reduction, 0..100
    inline constexpr const char* gain = "gain";       // Gain, 0..100 (0..40 dB)
    inline constexpr const char* mode = "mode";       // 0 Compress, 1 Limit
    inline constexpr const char* meter = "meter";     // 0 GR, 1 +10, 2 +4
    inline constexpr const char* emph = "emph";       // HF Emphasis, 0..100
    inline constexpr const char* mix = "mix";         // 0..100 percent wet
    inline constexpr const char* bypass = "bypass";
}

// Plugin-wide settings kept in the saved state (not presets, not automatable).
namespace setting
{
    inline constexpr const char* look = "look";       // "A", "B", "C"
    inline constexpr const char* tips = "tips";       // info bubbles
    inline constexpr const char* power = "power";     // power saving redraw
    inline constexpr const char* noise = "noise";     // hardware noise floor and hum
    inline constexpr const char* scale = "scale";     // window scale, 1.0 = 100 %
    inline constexpr const char* preset = "preset";   // name of the loaded preset
}

class LapCat2AProcessor : public juce::AudioProcessor
{
public:
    LapCat2AProcessor();
    ~LapCat2AProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LapCat2A"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter data for the editor (written on the audio thread, read by the editor timer).
    std::atomic<float> grAvgDb { 0.0f };     // average gain reduction of the last block, positive dB
    std::atomic<float> grPeakDb { 0.0f };    // largest gain reduction since the editor last read it (exchange to 0)
    std::atomic<float> vuEnergy { 0.0f };    // slow mean square of the output
    std::atomic<float> detInDb { -90.0f };   // detector input level, dB (transfer curve dot)
    std::atomic<float> grNowDb { 0.0f };     // instantaneous gain reduction, dB
    std::atomic<float> inPeak { 0.0f };      // linear peak since the editor last read it (exchange to 0), input
    std::atomic<float> outPeak { 0.0f };     // same for the output
    std::atomic<uint32_t> blocks { 0 };      // counts processed blocks, so the editor can tell when audio has stopped

    // Settings stored in the state tree.
    juce::var getSetting (const char* key, const juce::var& fallback) const;
    void setSetting (const char* key, const juce::var& v);
    static void raiseTo (std::atomic<float>& a, float v) { float c = a.load(); while (v > c && ! a.compare_exchange_weak (c, v)) {} }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    lap::Core core;
    std::atomic<bool> noiseRt { false }; // audio-thread copy of the noise setting
    std::atomic<float>* pPeak; std::atomic<float>* pGain; std::atomic<float>* pMode;
    std::atomic<float>* pEmph; std::atomic<float>* pMix; std::atomic<float>* pBypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LapCat2AProcessor)
};
