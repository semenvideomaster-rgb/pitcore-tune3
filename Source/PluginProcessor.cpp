#include "PluginProcessor.h"

PitCoreProcessor::PitCoreProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PitCoreProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto F = [&] (const char* id, const char* name, float lo, float hi, float def, float step = 0.f)
    { p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, step), def)); };

    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "key", 1 }, "Тональность",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "scale", 1 }, "Лад",
        juce::StringArray { "Хроматика", "Мажор", "Минор" }, 1));
    F ("strength", "Тюн: сила",       0.f, 1.f,   0.9f);
    F ("speed",    "Тюн: скорость, мс", 1.f, 200.f, 20.f, 1.f);
    F ("mix",      "Тюн: микс",       0.f, 1.f,   1.f);
    F ("eqLow",    "Neve: НЧ, дБ",   -12.f, 12.f, 0.f, 0.5f);
    F ("eqMid",    "Neve: Ср, дБ",   -12.f, 12.f, 0.f, 0.5f);
    F ("eqHigh",   "Neve: ВЧ, дБ",   -12.f, 12.f, 0.f, 0.5f);
    F ("chMix",    "Хорус: микс",     0.f, 1.f,   0.f);
    F ("chRate",   "Хорус: скорость, Гц", 0.1f, 3.f, 0.6f);
    F ("chDepth",  "Хорус: глубина",  0.f, 1.f,   0.5f);
    F ("dlyMix",   "Делей: микс",     0.f, 1.f,   0.f);
    F ("dlyTime",  "Делей: время, мс", 40.f, 900.f, 280.f, 1.f);
    F ("dlyFb",    "Делей: повторы",  0.f, 0.9f,  0.35f);
    F ("revMix",   "Реверб: микс",    0.f, 1.f,   0.12f);
    F ("revSize",  "Реверб: размер",  0.f, 1.f,   0.5f);
    F ("out",      "Выход, дБ",      -24.f, 12.f, 0.f, 0.5f);
    return { p.begin(), p.end() };
}

bool PitCoreProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void PitCoreProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    core.prepare (sr);
    setLatencySamples (core.latency());
    dryBuf.assign ((size_t) std::max (1, core.latency()), 0.f); dryPos = 0;

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };
    chorus.prepare (spec); chorus.reset(); chorus.setCentreDelay (12.f); chorus.setFeedback (0.f);
    reverb.setSampleRate (sr); reverb.reset();
    for (int k = 0; k < 3; ++k) { eq[k].reset(); lastEq[k] = 1e9f; }

    const size_t n = (size_t) (sr * 2.0) + 4;
    dlyL.assign (n, 0.f); dlyR.assign (n, 0.f); dlyPos = 0;
    dlyTime.reset (sr, 0.08); dlyTime.setCurrentAndTargetValue (param ("dlyTime"));
    outGain.reset (sr, 0.03); outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param ("out")));
    work.setSize (2, std::max (samplesPerBlock, 512));
}

void PitCoreProcessor::updateEq (int band, float gainDb)
{
    if (gainDb == lastEq[band]) return;
    lastEq[band] = gainDb;
    const float g = juce::Decibels::decibelsToGain (gainDb);
    using C = juce::dsp::IIR::Coefficients<float>;
    eq[band].coefficients = band == 0 ? C::makeLowShelf (sr, 120.0, 0.7f, g)
                          : band == 1 ? C::makePeakFilter (sr, 1600.0, 0.9f, g)
                                      : C::makeHighShelf (sr, 7000.0, 0.7f, g);
}

void PitCoreProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    if (n > work.getNumSamples()) work.setSize (2, n, false, false, true);

    core.key = (int) param ("key"); core.scale = (int) param ("scale");
    core.strength = param ("strength"); core.speedMs = param ("speed");
    const float mix = param ("mix");
    for (int k = 0; k < 3; ++k) updateEq (k, param (k == 0 ? "eqLow" : k == 1 ? "eqMid" : "eqHigh"));

    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);
    const float* in0 = buffer.getReadPointer (0);
    const float* in1 = nch > 1 ? buffer.getReadPointer (1) : in0;
    const int dryLen = (int) dryBuf.size();

    for (int i = 0; i < n; ++i)
    {
        const float x = 0.5f * (in0[i] + in1[i]);
        const float wet = core.process (x);
        const float dry = dryBuf[(size_t) dryPos];
        dryBuf[(size_t) dryPos] = x;
        if (++dryPos >= dryLen) dryPos = 0;
        float y = dry * (1.f - mix) + wet * mix;
        for (int k = 0; k < 3; ++k) y = eq[k].processSample (y);
        L[i] = R[i] = y;
    }

    const float chMix = param ("chMix");
    if (chMix > 0.001f)
    {
        chorus.setMix (chMix); chorus.setRate (param ("chRate")); chorus.setDepth (param ("chDepth"));
        juce::dsp::AudioBlock<float> blk (work);
        auto sub = blk.getSubBlock (0, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        chorus.process (ctx);
    }

    const float dMix = param ("dlyMix"), fb = param ("dlyFb");
    dlyTime.setTargetValue (param ("dlyTime"));
    const int size = (int) dlyL.size();
    for (int i = 0; i < n; ++i)
    {
        const float d = dlyTime.getNextValue() * (float) sr * 0.001f;
        float rp = (float) dlyPos - d; while (rp < 0.f) rp += (float) size;
        const int i0 = (int) rp % size, i1 = (i0 + 1) % size; const float f = rp - std::floor (rp);
        const float dl = dlyL[(size_t) i0] * (1.f - f) + dlyL[(size_t) i1] * f;
        const float dr = dlyR[(size_t) i0] * (1.f - f) + dlyR[(size_t) i1] * f;
        const float xl = L[i], xr = R[i];
        dlyL[(size_t) dlyPos] = xl + fb * dr; dlyR[(size_t) dlyPos] = xr + fb * dl;
        if (++dlyPos >= size) dlyPos = 0;
        L[i] = xl + dMix * dl; R[i] = xr + dMix * dr;
    }

    const float rv = param ("revMix");
    if (rv > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.65f * param ("revSize"); rp.damping = 0.5f; rp.width = 1.f;
        rp.wetLevel = rv * 0.5f; rp.dryLevel = 1.f - rv * 0.25f; rp.freezeMode = 0.f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, n);
    }

    outGain.setTargetValue (juce::Decibels::decibelsToGain (param ("out")));
    float* o0 = buffer.getWritePointer (0);
    float* o1 = nch > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        if (o1) { o0[i] = L[i] * g; o1[i] = R[i] * g; }
        else    { o0[i] = 0.5f * (L[i] + R[i]) * g; }
    }
}

void PitCoreProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void PitCoreProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitCoreProcessor(); }
