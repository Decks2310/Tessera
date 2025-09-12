//================================================================================
// File: DSP_Helpers/SpectralAnalyzer.cpp
//================================================================================
#include "SpectralAnalyzer.h"

SpectralAnalyzer::SpectralAnalyzer()
    : fft(FFT_ORDER),
    // JUCE 8: Initialize using the WindowingMethod enum.
    window(FFT_SIZE, juce::dsp::WindowingFunction<float>::WindowingMethod::hann)
{
}

void SpectralAnalyzer::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    // Initialize buffers
    inputFIFO.resize(FFT_SIZE, 0.0f);
    fftData.resize(FFT_SIZE * 2, 0.0f); // JUCE requires 2*N for real-only FFT

    // Initialize smoothing (e.g., 30ms response time)
    smoothedCentroid.reset(spec.sampleRate, 0.03);

    reset();
}

void SpectralAnalyzer::reset() {
    fifoIndex = 0;
    std::fill(inputFIFO.begin(), inputFIFO.end(), 0.0f);
    smoothedCentroid.setCurrentAndTargetValue(0.5f); // Start neutral
}

// FIX: Removed process(block) and replaced with processSample(monoSample)
// void SpectralAnalyzer::process(const juce::dsp::AudioBlock<float>& block) { ... } // REMOVED

// NEW IMPLEMENTATION: Process sample-by-sample
void SpectralAnalyzer::processSample(float monoSample) {
    // Safety check for buffer size
    if (fifoIndex >= (int)inputFIFO.size())
    {
        // Advance smoother anyway to maintain timing.
        smoothedCentroid.getNextValue();
        return;
    }

    // Push sample into FIFO
    inputFIFO[fifoIndex] = monoSample;
    fifoIndex++;

    // Check if a frame is ready
    if (fifoIndex == FFT_SIZE)
    {
        processFrame();
        // Shift FIFO for overlap (OLA implementation)
        // Ensure inputFIFO is large enough before copying (safety check)
        if ((int)inputFIFO.size() >= FFT_SIZE)
        {
            std::copy(inputFIFO.begin() + HOP_SIZE, inputFIFO.end(), inputFIFO.begin());
        }
        fifoIndex = FFT_SIZE - HOP_SIZE; // Corrected index after shift
    }

    // CRITICAL: Advance the smoother sample-by-sample for accurate control signal timing
    smoothedCentroid.getNextValue();
}

// Blueprint 1.2: Spectral Centroid Calculation
void SpectralAnalyzer::processFrame() {
    // 1. Copy FIFO to FFT buffer and apply window
    std::copy(inputFIFO.begin(), inputFIFO.end(), fftData.begin());
    window.multiplyWithWindowingTable(fftData.data(), FFT_SIZE);

    // 2. Perform Forward FFT
    fft.performRealOnlyForwardTransform(fftData.data());

    // 3. Calculate Spectral Centroid
    float weightedSum = 0.0f;
    float totalSum = 0.0f;
    int numBins = FFT_SIZE / 2 + 1;
    float nyquist = (float)sampleRate * 0.5f;

    for (int i = 1; i < numBins; ++i) // Start from bin 1 (skip DC)
    {
        float real, imag;
        // Unpack JUCE format
        if (i == FFT_SIZE / 2) { real = fftData[1]; imag = 0.0f; } // Nyquist
        else { real = fftData[2 * i]; imag = fftData[2 * i + 1]; }

        float magnitude = std::sqrt(real * real + imag * imag);

        // Calculate frequency of the bin
        float freq = (float)i * (float)sampleRate / (float)FFT_SIZE;

        weightedSum += magnitude * freq;
        totalSum += magnitude;
    }

    // 4. Normalize and set target
    if (totalSum > 1e-6f)
    {
        float centroidFreq = weightedSum / totalSum;
        // Normalize centroid relative to Nyquist
        float normalizedCentroid = centroidFreq / nyquist;
        smoothedCentroid.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedCentroid));
    }
}
