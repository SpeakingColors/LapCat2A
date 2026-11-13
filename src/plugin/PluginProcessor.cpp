#include "PluginProcessor.h"
#include "../editor/PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout LapCat2AProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const int v = 1; // parameter version
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::peak, v }, "Peak Reduction",
        NormalisableRange<float> (0.0f, 100.0f, 0.01f), 50.0f)); // default = the Start Here preset
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::gain, v }, "Gain",
        NormalisableRange<float> (0.0f, 100.0f, 0.01f), 14.0f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::mode, v }, "Ratio Mode",
        StringArray { "Compress", "Limit" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::meter, v }, "Meter",
        StringArray { "GR", "+10", "+4" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::emph, v }, "HF Emphasis",
        NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, v }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, v }, "Bypass", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::hq, v }, "High Quality 8x", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::link, v }, "Stereo Link", true));
    return { p.begin(), p.end() };
}

LapCat2AProcessor::LapCat2AProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LAPCAT2A", createLayout())
{
    pPeak   = apvts.getRawParameterValue (ids::peak);
    pGain   = apvts.getRawParameterValue (ids::gain);
    pMode   = apvts.getRawParameterValue (ids::mode);
    pEmph   = apvts.getRawParameterValue (ids::emph);
    pMix    = apvts.getRawParameterValue (ids::mix);
    pBypass = apvts.getRawParameterValue (ids::bypass);
    pHq     = apvts.getRawParameterValue (ids::hq);
    pLink   = apvts.getRawParameterValue (ids::link);
}

bool LapCat2AProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

var LapCat2AProcessor::getSetting (const char* key, const var& fallback) const
{
    return apvts.state.getProperty (key, fallback);
}

void LapCat2AProcessor::setSetting (const char* key, const var& v)
{
    apvts.state.setProperty (key, v, nullptr);
    if (String (key) == setting::noise) noiseRt.store ((bool) v);
}

void LapCat2AProcessor::prepareToPlay (double sampleRate, int)
{
    core.prepare (sampleRate);
    lap::Params q;
    q.peak = pPeak->load(); q.gain = pGain->load(); q.limit = pMode->load() > 0.5f ? 1.0f : 0.0f;
    q.emph = pEmph->load(); q.mix = pMix->load(); q.bypass = pBypass->load() > 0.5f; q.noise = noiseRt.load();
    q.hq = pHq->load() > 0.5f; q.link = pLink->load() > 0.5f;
    core.setParams (q);
    core.snap();
    setLatencySamples (core.latency());
}

void LapCat2AProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numCh = jmin (2, buffer.getNumChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh == 0) return;
    const int n = buffer.getNumSamples();

    lap::Params q;
    q.peak = pPeak->load(); q.gain = pGain->load(); q.limit = pMode->load() > 0.5f ? 1.0f : 0.0f;
    q.emph = pEmph->load(); q.mix = pMix->load(); q.bypass = pBypass->load() > 0.5f; q.noise = noiseRt.load();
    q.hq = pHq->load() > 0.5f; q.link = pLink->load() > 0.5f;
    core.setParams (q);

    float inPk = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* x = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i) inPk = jmax (inPk, std::abs (x[i]));
    }
    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    core.process (l, r, n);
    float outPk = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* x = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i) outPk = jmax (outPk, std::abs (x[i]));
    }

    blocks.fetch_add (1, std::memory_order_relaxed);
    grAvgDb.store ((float) core.grAvg());
    raiseTo (grPeakDb, (float) core.grPeak());
    vuEnergy.store ((float) core.vuEnergy());
    detInDb.store ((float) core.detectorIn());
    grNowDb.store ((float) core.grNow());
    raiseTo (inPeak, inPk);
    raiseTo (outPeak, outPk);
}

AudioProcessorEditor* LapCat2AProcessor::createEditor()
{
    return new LapCat2AEditor (*this);
}

void LapCat2AProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void LapCat2AProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml)); // unknown keys are ignored by APVTS
            noiseRt.store ((bool) getSetting (setting::noise, false));
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LapCat2AProcessor(); }
