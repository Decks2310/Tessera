//================================================================================
// File: FX_Modules/SpectralDiffuser.h
//================================================================================
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

class SpectralDiffuser
{
public:
    // OPTIMIZATION: Reduced FFT Order from 11 (2048) to 10 (1024).
    // This significantly reduces CPU load while still providing effective diffusion.
    static constexpr int FFT_ORDER = 10;
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int HOP_SIZE = FFT_SIZE / 2; // 50% Overlap

    // JUCE 8 FIX (C2039/C2065): Initialize using the WindowingMethod enum and a standard window.
    SpectralDiffuser()
        : fft(FFT_ORDER),
        window(FFT_SIZE, juce::dsp::WindowingFunction<float>::WindowingMethod::hann),
        distribution(-juce::MathConstants<float>::pi, juce::MathConstants<float>::pi)
    {
        // Ensure unique seeding
        randomEngine.seed(static_cast<unsigned long>(juce::Time::currentTimeMillis()));
    }


    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float diffusionAmount);
    int getLatencyInSamples() const { return HOP_SIZE; }

private:
    void processFrame(int channel, float diffusionAmount);

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    juce::AudioBuffer<float> inputFIFO;
    juce::AudioBuffer<float> outputFIFO;
    std::vector<std::vector<float>> fftData;
    int fifoIndex = 0;

    std::minstd_rand randomEngine;
    std::uniform_real_distribution<float> distribution;
};