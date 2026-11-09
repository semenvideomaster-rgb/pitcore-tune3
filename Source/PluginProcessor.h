#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AutoTuneCore.h"

class PitCoreProcessor : public juce::AudioProcessor
{
public:
    PitCoreProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Pit Core Tune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void updateEq (int band, float gainDb);

    AutoTuneCore core;
    double sr = 44100.0;
    std::vector<float> dryBuf; int dryPos = 0;
    juce::dsp::IIR::Filter<float> eq[3];
    float lastEq[3] { 1e9f, 1e9f, 1e9f };
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;
    std::vector<float> dlyL, dlyR; int dlyPos = 0;
    juce::SmoothedValue<float> dlyTime, outGain;
    juce::AudioBuffer<float> work;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitCoreProcessor)
};
