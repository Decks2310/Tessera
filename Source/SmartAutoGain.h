//================================================================================
// File: SmartAutoGain.h
//================================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "LUFSMeter.h"

/**
 * State-of-the-Art Auto-Gain System based on differential Momentary LUFS matching.
 */
class SmartAutoGain
{
public:
    SmartAutoGain();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    // New Unified process method: Analyzes dry, analyzes wet, and applies gain to wetBlock.
    void process(const juce::dsp::AudioBlock<float>& dryBlock, juce::dsp::AudioBlock<float>& wetBlock);
    // The new architecture has zero latency.
    int getLatencyInSamples() const { return 0; }

    void setEnabled(bool isEnabled);
    // New Control: Allows user adjustment of the smoothing speed.
    void setResponseTime(float timeMs);
private:
    // Removed all inner classes (AnalysisEngine, DualPathProcessor, DynamicEQ, TruePeakLimiter)

    bool enabled = false;
    double sampleRate = 44100.0;

    // Meters for input (dry) and output (wet) signals
    LUFSMeter inputLoudnessMeter;
    LUFSMeter wetLoudnessMeter;
    // Smoother for the enable/disable bypass fade (Linear is fine for bypass mixing)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> enabledSmoother;
    // Gain smoother using Multiplicative smoothing (Psychoacoustically correct for gain changes)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmoother;
    float responseTimeMs = 50.0f; // Default response time (50ms)
};