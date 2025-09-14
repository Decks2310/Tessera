//================================================================================
// File: FX_Modules/SpectralDiffuser.h
//================================================================================
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <random>
#include <vector>

class SpectralDiffuser
{
public:
    static constexpr int FFT_ORDER = 10;
    static constexpr int FFT_SIZE  = 1 << FFT_ORDER;
    static constexpr int HOP_SIZE  = FFT_SIZE / 2;

    SpectralDiffuser();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float diffusionAmount);
    int  getLatencyInSamples() const { return HOP_SIZE; }

    void setPhaseDriftScale(float s) { phaseDriftScale = juce::jlimit(0.0f, 4.0f, s); }
    void setNormalizeOutput(bool b) { normalizeOutput = b; }

private:
    void processFrame(int channel, float diffusionAmount);

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    juce::AudioBuffer<float> inputFIFO, outputFIFO;
    std::vector<std::vector<float>> fftData;
    int fifoIndex = 0;

    std::minstd_rand randomEngine;
    std::uniform_real_distribution<float> distribution;

    float phaseDriftScale = 1.0f;
    float prevDiffusion   = 0.0f;
    bool  normalizeOutput = true;
};