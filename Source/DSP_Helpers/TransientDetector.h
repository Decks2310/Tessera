//================================================================================
// File: DSP_Helpers/TransientDetector.h
//================================================================================
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <algorithm>
#include <cmath>

/**
 * TransientDetector using Spectral Flux method.
 */
class TransientDetector
{
public:
    // Configuration: FFT size 512 (Order 9). 50% overlap.
    static constexpr int FFT_ORDER = 9;
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int HOP_SIZE = FFT_SIZE / 2;

    TransientDetector();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    /**
     * Process a single mono sample. This advances the internal state and the smoother.
     */
    void processSample(float monoSample);

    /**
     * Returns the current smoothed transient detection value (0.0 to 1.0).
     */
    float getTransientValue() const { return smoothedFlux.getCurrentValue(); }

    /**
     * Returns the latency introduced by the STFT process (equal to the Hop Size).
     */
    int getLatencyInSamples() const { return HOP_SIZE; }

private:
    void processFrame();

    // DSP Components
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    // Buffering (Mono)
    std::vector<float> inputFIFO;
    std::vector<float> fftData;
    std::vector<float> currentMagnitudes;
    std::vector<float> previousMagnitudes;
    int fifoIndex = 0;

    // Output smoothing
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFlux;
};