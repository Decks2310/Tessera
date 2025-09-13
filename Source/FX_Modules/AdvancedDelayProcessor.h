// File: FX_Modules/AdvancedDelayProcessor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSPUtils.h"

class AdvancedDelayProcessor : public juce::AudioProcessor
{
public:
    enum class DelayMode { Tape, BBD, Digital };

    AdvancedDelayProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~AdvancedDelayProcessor() override = default;

    const juce::String getName() const override { return "Advanced Delay"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Standard JUCE Boilerplate
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    void processTapeMode(juce::AudioBuffer<float>& buffer);
    void processDigitalMode(juce::AudioBuffer<float>& buffer);

    // --- Core Components ---
    // Upgraded interpolation (Blueprint 2.2.1)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine;
    double currentSampleRate = 44100.0;

    // --- Tape Mode Components (Blueprint 2.2) ---
    DSPUtils::LFO wowLFO;
    DSPUtils::LFO flutterLFO;
    DSPUtils::NoiseGenerator noiseSource;

    juce::dsp::WaveShaper<float> tapeSaturator;
    juce::dsp::StateVariableTPTFilter<float> tapeFilters;

    // --- Parameters ---
    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String modeParamId, timeParamId, feedbackParamId, mixParamId, colorParamId, wowParamId, flutterParamId, ageParamId;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedTimeMs;
};