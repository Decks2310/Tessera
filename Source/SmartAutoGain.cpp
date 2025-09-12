//================================================================================
// File: SmartAutoGain.cpp
//================================================================================
#include "SmartAutoGain.h"

SmartAutoGain::SmartAutoGain() {}

void SmartAutoGain::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate = spec.sampleRate;
    inputLoudnessMeter.prepare(spec);
    wetLoudnessMeter.prepare(spec);
    // Fast 10ms fade for enable/disable transitions
    enabledSmoother.reset(spec.sampleRate, 0.01);
    // Initialize gain smoother with the current response time
    setResponseTime(responseTimeMs);

    reset();
}

void SmartAutoGain::reset() {
    inputLoudnessMeter.reset();
    wetLoudnessMeter.reset();

    enabledSmoother.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
    gainSmoother.setCurrentAndTargetValue(1.0f); // Start with unity gain
}

// New implementation of the core differential logic
void SmartAutoGain::process(const juce::dsp::AudioBlock<float>& dryBlock, juce::dsp::AudioBlock<float>& wetBlock) {

    // Update bypass smoother target
    enabledSmoother.setTargetValue(enabled ? 1.0f : 0.0f);
    // --- BYPASS LOGIC ---
    // If completely disabled and the fade-out is finished, do nothing.
    if (!enabled && !enabledSmoother.isSmoothing() && enabledSmoother.getCurrentValue() < 1e-6f)
    {
        // Ensure gain smoother resets to unity if we bypass, just in case.
        if (gainSmoother.getCurrentValue() != 1.0f)
            gainSmoother.setCurrentAndTargetValue(1.0f);
        return;
    }

    // Ensure block sizes match for safety
    // === FIX C4267: Use int for sample count to address warnings ===
    int numSamples = (int)juce::jmin(dryBlock.getNumSamples(), wetBlock.getNumSamples());
    // ===============================================================

    if (numSamples == 0) return;

    // getSubBlock requires size_t for the length argument.
    auto drySubBlock = dryBlock.getSubBlock(0, (size_t)numSamples);
    auto wetSubBlock = wetBlock.getSubBlock(0, (size_t)numSamples);
    // 1. Analyze both dry and wet signals (Always analyze to keep meters updated)
    inputLoudnessMeter.process(drySubBlock);
    wetLoudnessMeter.process(wetSubBlock);
    // 2. Calculate loudness delta
    float dryLufs = inputLoudnessMeter.getMomentaryLoudness();
    float wetLufs = wetLoudnessMeter.getMomentaryLoudness();
    // Define a noise floor to prevent extreme gain with silent input
    constexpr float silenceThresholdLufs = -70.0f;
    float targetGain = 1.0f;

    if (dryLufs > silenceThresholdLufs && wetLufs > silenceThresholdLufs)
    {
        // Calculate the difference: Gain needed = Dry LUFS - Wet LUFS
        float loudnessDeltaDb = dryLufs - wetLufs;
        // Clamp the correction to a reasonable range (e.g., +/- 24dB) for safety
        loudnessDeltaDb = juce::jlimit(-24.0f, 24.0f, loudnessDeltaDb);
        // Convert dB difference to linear gain factor
        targetGain = juce::Decibels::decibelsToGain(loudnessDeltaDb);
    }

    // 3. Set the target for the gain smoother
    gainSmoother.setTargetValue(targetGain);
    // 4. Apply the smoothed gain to the wet block
    // We combine the auto-gain value with the bypass fade value for smooth transitions.
    // We must process per-sample if either the gain smoother OR the bypass smoother is active.
    if (gainSmoother.isSmoothing() || enabledSmoother.isSmoothing())
    {
        // === FIX C4267: Use int for channel count and loop indices ===
        int numChannels = (int)wetSubBlock.getNumChannels();
        for (int i = 0; i < numSamples; ++i)
        {
            // Get the current auto-gain value (multiplicative smoothing)
            float autoGain = gainSmoother.getNextValue();
            // Get the current bypass fade value (linear smoothing: 1.0=active, 0.0=bypassed)
            float bypassMix = enabledSmoother.getNextValue();
            // Calculate the final applied gain: Interpolate between unity (1.0) and autoGain.
            // When bypassMix is 1.0, finalGain = autoGain.
            // When bypassMix is 0.0, finalGain = 1.0.
            float finalGain = (autoGain * bypassMix) + (1.0f * (1.0f - bypassMix));
            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Use int indices (ch, i) as required by the compiler in this context.
                wetSubBlock.setSample(ch, i, wetSubBlock.getSample(ch, i) * finalGain);
            }
        }
        // ===============================================================
    }
    else if (enabled)
    {
        // Optimization: If fully enabled and gain is settled, we can use the faster block-based operation.
        // Since we checked isSmoothing() == false, we know the gain is constant.
        // === FIX C2665: SmoothedValue::applyGain does not accept AudioBlock. ===
        // Use AudioBlock::multiplyBy() instead.
        // OLD: gainSmoother.applyGain(wetSubBlock, (int)numSamples);
        wetSubBlock.multiplyBy(gainSmoother.getCurrentValue());
        // =======================================================================
    }
}

void SmartAutoGain::setEnabled(bool isEnabled) {
    enabled = isEnabled;
    // Target value update is handled at the start of the process loop
}

// New Control Implementation
void SmartAutoGain::setResponseTime(float timeMs) {
    // Enforce a practical range (e.g., 10ms min, 1000ms max)
    responseTimeMs = juce::jlimit(10.0f, 1000.0f, timeMs);
    // Update the smoother's ramp length based on the new time (converted to seconds)
    if (sampleRate > 0)
        gainSmoother.reset(sampleRate, responseTimeMs / 1000.0);
}