#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSPUtils.h"
// NEW: Include the robust analysis helpers
#include "../DSP_Helpers/SpectralAnalyzer.h"
#include "../DSP_Helpers/TransientDetector.h"

// REMOVED: Internal flawed SignalAnalyzer class definition.

// [TopologyParams struct and Topologies namespace - Identical to input, omitted for brevity]
struct TopologyParams
{
    float attackFactor;
    float releaseFactor;
    float ratioFactor;
    float saturationDrive;
    float (*saturationFunc)(float);
};

namespace Topologies
{
    static float vcaSaturation(float x) { return std::tanh(x); }
    static float fetSaturation(float x) { return x / (std::abs(x) + 0.7f); }
    static float optoSaturation(float x) { return std::tanh(x * 0.8f); }
    static float varimuSaturation(float x) { return std::tanh(x * 1.5f); }

    const TopologyParams VCA = { 1.0f, 1.0f, 1.0f, 0.5f, vcaSaturation };
    const TopologyParams FET = { 0.2f, 0.8f, 1.5f, 1.5f, fetSaturation };
    const TopologyParams Opto = { 2.0f, 1.5f, 0.8f, 0.2f, optoSaturation };
    const TopologyParams VariMu = { 1.5f, 2.0f, 0.9f, 1.0f, varimuSaturation };
}


class MorphoCompProcessor : public juce::AudioProcessor
{
public:
    MorphoCompProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~MorphoCompProcessor() override = default;

    const juce::String getName() const override { return "MorphoComp"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // [JUCE Boilerplate - Identical to input]
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    // Increased tail length slightly due to analysis latency and release times.
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    void updateCompressorAndSaturation(float amount, float response, float morphX, float morphY);

    // UPDATED: Use the robust analysis helpers instead of the internal analyzer
    // SignalAnalyzer analyzer; // REMOVED
    SpectralAnalyzer spectralAnalyzer;
    TransientDetector transientDetector;

    juce::dsp::Compressor<float> compressor;
    juce::dsp::WaveShaper<float> saturator;

    // Use Linear smoothing for the control signals (X/Y)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> morphXSmoother, morphYSmoother;
    float currentSaturationDrive = 1.0f;

    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String amountParamId, responseParamId, modeParamId, morphXParamId, morphYParamId, mixParamId;
};
