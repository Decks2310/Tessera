//================================================================================
// File: LUFSMeter.h
//================================================================================
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <deque>

class LUFSMeter
{
public:
    LUFSMeter();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::dsp::AudioBlock<float>& block);
    // Short-Term Loudness removed for efficiency
    float getMomentaryLoudness() const;
private:
    void applyKWeighting(juce::dsp::AudioBlock<float>& block);
    void updateGatedLoudness(const juce::dsp::AudioBlock<float>& block);

    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;

    // K-Weighting Filters as per BS.1770-5
    Filter stage1Filter; // High-shelf (head model)
    Filter stage2Filter; // High-pass

    // Buffer for K-weighted signal
    juce::AudioBuffer<float> weightedBuffer;
    // Integration management (Optimized for Momentary only)
    static constexpr int momentaryIntegrationMs = 400;
    int momentarySamples = 0;
    // Buffer storing the mean square values over the integration window
    std::deque<float> momentaryBlockLoudness;

    float currentMomentaryLoudness = -144.0f;
};