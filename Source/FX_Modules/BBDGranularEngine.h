//================================================================================
// File: FX_Modules/BBDGranularEngine.h
//================================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <random>
#include <vector>
#include <cmath> // Added for std::cos
#include "../DSP_Helpers/InterpolatedCircularBuffer.h"
#include "../DSPUtils.h" // Required for NoiseGenerator and fastTanh
class BBDGranularEngine
{
public:
    // OPTIMIZATION: Set MAX_GRAINS to 64. This provides a good balance of density and performance
    // given the reduced spawn rates configured in BBDCloudProcessor.cpp.
    static constexpr int MAX_GRAINS = 64;

    // Configuration structure (remains the same)
    struct Config
    {
        float minDurationMs = 10.0f;
        float maxDurationMs = 100.0f;
        float baseCutoffHz = 5000.0f;
        float saturationDrive = 1.2f;
        float spawnRateHzMax = 500.0f;
        float noiseAmount = 0.05f;
    };

    BBDGranularEngine();
    void prepare(const juce::dsp::ProcessSpec& spec, const Config& newConfig, int maxBufferSizeSamples);
    void reset();
    void capture(const juce::dsp::AudioBlock<float>& inputBlock);
    void process(juce::dsp::AudioBlock<float>& outputBlock, float density, float timeMs, float spread, float age);

private:
    void spawnGrain(float timeMs, float spread, float age);

    struct Grain
    {
        bool isActive = false;
        int durationSamples = 0;
        float grainPhase = 0.0f; // 0.0 to 1.0
        float bufferReadPosition = 0.0f;

        // Stochastic Parameters
        float amplitude = 1.0f;
        float pan = 0.5f; // 0=L, 1=R

        // Per-Grain DSP (BBD Emulation)

        // ARTIFACT FIX: Upgraded from FirstOrderTPTFilter (6dB/oct) to StateVariableTPTFilter (12dB/oct).
        // Steeper filtering is essential for anti-aliasing in BBD emulation.
        // OLD: juce::dsp::FirstOrderTPTFilter<float> filterL, filterR;
        juce::dsp::StateVariableTPTFilter<float> filterL, filterR;

        float pitchRatio = 1.0f;
        float noiseLevel = 0.0f;

        static float applyTukeyWindow(float phase);
    };

    // --- Members ---
    double sampleRate = 44100.0;
    int numChannels = 2;
    Config config;
    InterpolatedCircularBuffer captureBuffer;
    std::vector<Grain> grains;

    // Spawning control
    float samplesUntilNextGrain = 0.0f;

    // Randomization
    std::minstd_rand randomEngine;
    std::uniform_real_distribution<float> distribution{ 0.0f, 1.0f };
    DSPUtils::NoiseGenerator noiseGen;
};