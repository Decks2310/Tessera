#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSPUtils.h" // adjust if build system expects different relative path

class DistortionProcessor : public juce::AudioProcessor
{
public:
    DistortionProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~DistortionProcessor() override = default;

    const juce::String getName() const override { return "Distortion"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    enum class Algo { VintageTube, OpAmp, GermaniumFuzz };

    juce::dsp::Gain<float> preGain;
    juce::dsp::Gain<float> postGain;

    using DCFilter = juce::dsp::IIR::Filter<float>;
    using DCFilterState = juce::dsp::IIR::Coefficients<float>;
    juce::dsp::ProcessorDuplicator<DCFilter, DCFilterState> inputDCBlocker;
    juce::dsp::ProcessorDuplicator<DCFilter, DCFilterState> outputDCBlocker;

    // Removed localOversampler: global/master oversampling handles rate increase.
    DSPUtils::EnvelopeFollower inputFollower;

    float processTube(float x, float bias, float dynamicBias);
    float processOpAmp(float x, float character);
    float processGermanium(float x, float stability);

    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String driveParamId, levelParamId, typeParamId, biasParamId, characterParamId;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedBias;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedCharacter;
};