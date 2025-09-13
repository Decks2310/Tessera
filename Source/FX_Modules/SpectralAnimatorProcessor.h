//================================================================================
// File: FX_Modules/SpectralAnimatorProcessor.h
//================================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SpectralAnimatorEngine.h"

class SpectralAnimatorProcessor : public juce::AudioProcessor
{
public:
    SpectralAnimatorProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~SpectralAnimatorProcessor() override = default;

    const juce::String getName() const override { return "Spectral Animator"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Standard JUCE Boilerplate
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    // The STFT process introduces latency and a tail.
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    // The DSP engine instance
    SpectralAnimatorEngine engine;

    juce::AudioProcessorValueTreeState& mainApvts;
    // Parameter IDs
    juce::String modeParamId, pitchParamId, formantXParamId, formantYParamId, morphParamId, transientParamId;
};