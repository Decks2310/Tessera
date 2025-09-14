//================================================================================
// File: FX_Modules/HelicalDelayProcessor.h (Project Copy)
//================================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Direct relative includes back to original project Source tree
// (Build copy resides at Builds/VisualStudio2022/Source/FX_Modules/...)
#include "../DSPUtils.h"
#include "../DSP_Helpers/InterpolatedCircularBuffer.h"

class HelicalDelayProcessor : public juce::AudioProcessor
{
public:
    HelicalDelayProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex);
    ~HelicalDelayProcessor() override = default;

    const juce::String getName() const override { return "Helical Delay"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    InterpolatedCircularBuffer delayBuffer;
    using Filter = juce::dsp::StateVariableTPTFilter<float>;
    Filter degradeFilter;
    DSPUtils::LFO textureLFO;

    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String timeParamId, pitchParamId, feedbackParamId, degradeParamId, textureParamId, mixParamId;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedTimeMs;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedPitch;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedFeedback;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDegrade;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedTexture;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix;

    double currentSampleRate = 44100.0;
    std::vector<double> readPositions;
};
