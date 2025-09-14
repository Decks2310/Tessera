//================================================================================
// File: DSP_Helpers/SpectralAnalyzer.h
//================================================================================
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <algorithm>
#include <cmath>

class SpectralAnalyzer
{
public:
    // Configuration: FFT size 512 provides a good balance of time/frequency resolution and efficiency.
    static constexpr int FFT_ORDER = 9;
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int HOP_SIZE = FFT_SIZE / 2; // 50% overlap

    SpectralAnalyzer();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // FIX: Changed interface to process sample-by-sample
    // void process(const juce::dsp::AudioBlock<float>& block); // REMOVED
    void processSample(float monoSample);

    // Returns the current smoothed spectral centroid (0.0 = low/dark, 1.0 = high/bright).
    float getSpectralCentroid() const { return smoothedCentroid.getCurrentValue(); }
private:
    void processFrame();

    double sampleRate = 44100.0;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    // Buffering for OLA (Mono analysis)
    std::vector<float> inputFIFO;
    std::vector<float> fftData;
    int fifoIndex = 0;

    // Output smoothing (Linear smoothing is appropriate for control signals)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedCentroid;
};
