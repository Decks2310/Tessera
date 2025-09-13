// File: FX_Modules/AdvancedCompressorProcessor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSPUtils.h"

class AdvancedCompressorProcessor : public juce::AudioProcessor
{
public:
    // Topology selection (Blueprint 3.3)
    enum class Topology { VCA_Clean, FET_Aggressive, Opto_Smooth };
    // Detector Modes (Blueprint 3.2.1)
    enum class DetectorMode { Peak, RMS };

    AdvancedCompressorProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~AdvancedCompressorProcessor() override = default;

    const juce::String getName() const override { return "Advanced Compressor"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Standard JUCE Boilerplate
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    // --- Detector Stage (Blueprint 3.2.1) ---
    juce::dsp::BallisticsFilter<float> peakDetector;

    // RMS detector implementation (Exponential Moving Average)
    float calculateRMS(int channel, float input);
    std::vector<float> rmsAverages;
    float rmsWindowTimeMs = 10.0f;
    float rmsAlpha = 0.99f;

    // --- Gain Computer ---
    float calculateGainDb(float detectorDb, float thresholdDb, float ratio);

    // --- Envelope Stage (Blueprint 3.2.2) ---
    juce::dsp::BallisticsFilter<float> envelopeSmoother;

    // --- Coloration stages (Blueprint 3.3) ---
    juce::dsp::WaveShaper<float> colorationStage;
    juce::dsp::Gain<float> makeupGain;

    // --- Parameters and State ---
    double currentSampleRate = 44100.0;

    void configureTopology(Topology topology, float attackMs, float releaseMs);

    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String topologyParamId, detectorParamId, thresholdParamId, ratioParamId, attackParamId, releaseParamId, makeupParamId;
};